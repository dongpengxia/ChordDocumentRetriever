#include "gu-chord.h"
#include "ns3/random-variable.h"
#include "ns3/inet-socket-address.h"

using namespace ns3;

TypeId
GUChord::GetTypeId ()
{
  static TypeId tid = TypeId ("GUChord")
    .SetParent<GUApplication> ()
    .AddConstructor<GUChord> ()
    .AddAttribute ("AppPort",
                   "Listening port for Application",
                   UintegerValue (10001),
                   MakeUintegerAccessor (&GUChord::m_appPort),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("PingTimeout",
                   "Timeout value for PING_REQ in milliseconds",
                   TimeValue (MilliSeconds (2000)),
                   MakeTimeAccessor (&GUChord::m_pingTimeout),
                   MakeTimeChecker ())
	
    //add periodic stabilization timer
    .AddAttribute ("StabilizeTimeout",
                   "Timeout value for Stabilize() in milliseconds",
                   TimeValue( MilliSeconds(3100)), //Use 5000000 to prevent output flooding, 20000 to stabilize often
                   MakeTimeAccessor(&GUChord::m_stabilizeTimeOut),
                   MakeTimeChecker ())
    //add periodic finger fixing timer
    .AddAttribute ("FixFingerTimeout",
                   "Timeout value for FixFingerTable() in milliseconds",
                   TimeValue( MilliSeconds(4000)), //Use 5000000 to prevent output flooding, 20000 to fix fingers often
                   MakeTimeAccessor(&GUChord::m_fixFingersTimeOut),
                   MakeTimeChecker ())
    ;
  return tid;
}

GUChord::GUChord ()
  : m_auditPingsTimer (Timer::CANCEL_ON_DESTROY)
{
  RandomVariable random;
  SeedManager::SetSeed (time (NULL));
  random = UniformVariable (0x00000000, 0xFFFFFFFF);
  m_currentTransactionId = random.GetInteger ();
}

GUChord::~GUChord ()
{
}

void
GUChord::DoDispose ()
{
  StopApplication ();
  GUApplication::DoDispose ();
}

void
GUChord::StartApplication (void)
{
  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny(), m_appPort);
      m_socket->Bind (local);
      m_socket->SetRecvCallback (MakeCallback (&GUChord::RecvMessage, this));
    }  
  // Configure timers
  m_auditPingsTimer.SetFunction (&GUChord::AuditPings, this);
  // Start timers
  m_auditPingsTimer.Schedule (m_pingTimeout);

  //initialize to local values
  m_localNodeNum = atoi(GetNodeId().c_str());
  m_localHashNum = hashIP(m_local);
  m_inChord = false;

  //set to default flag values, no successor or predecessor yet
  successor.ip = Ipv4Address::GetAny();
  successor.hashValue = numRingSlots + 1;
  predecessor.ip = Ipv4Address::GetAny();
  predecessor.hashValue = numRingSlots + 1;

  //set timer for periodic stabilization
  m_stabilizeTimer.SetFunction(&GUChord::stabilize, this);
  m_stabilizeTimer.Schedule(m_stabilizeTimeOut);

  //initialize empty finger table
  fingerTable = std::vector<hashNode> (fingerTableSize);

  for(uint32_t tableIndex = 0; tableIndex < fingerTableSize; tableIndex++)
  {
    fingerTable[tableIndex].ip = Ipv4Address::GetAny();
    fingerTable[tableIndex].hashValue = numRingSlots + 1;
  }

  //set timer for finger fixing
  m_fixFingersTimer.SetFunction(&GUChord::FixFingerTable, this);
  m_fixFingersTimer.Schedule(m_fixFingersTimeOut);
}

void
GUChord::StopApplication (void)
{
  //Close socket
  if (m_socket)
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
      m_socket = 0;
    }

  //Cancel timers
  m_auditPingsTimer.Cancel ();
  m_stabilizeTimer.Cancel();
  m_fixFingersTimer.Cancel();
  m_pingTracker.clear ();
}

void
GUChord::ProcessCommand (std::vector<std::string> tokens)
{
  std::vector<std::string>::iterator iterator = tokens.begin();
  std::string command = *iterator;

  //convert command to uppercase
  transform(command.begin(), command.end(), command.begin(), ::toupper);

  if(command == "INFO")
  {
    std::cout << std::endl << "Node number: " << m_localNodeNum << ", IP address: " << m_local << ", Chord Key: " << m_localHashNum << std::endl;
    std::cout << "Predecessor node number: " <<  m_addressNodeMap[predecessor.ip] << ", IP address: " << predecessor.ip;
    std::cout << ", Chord Key: " << predecessor.hashValue << std::endl;
    std::cout << "Successor node number: " << m_addressNodeMap[successor.ip] << ", IP address: " << successor.ip;
    std::cout << ", Chord Key: " << successor.hashValue << std::endl;

    PrintFingerTable();
  }
  else if(command == "JOIN")
  {
    //joining a chord ring
    m_inChord = true;
    iterator++;
    std::istringstream sin(*iterator);
    uint32_t nodeNumber;
    sin >> nodeNumber;

    //first node in ring
    if(nodeNumber == m_localNodeNum)
    {
      CreateChord();
    }
    //joining an existing ring
    else
    {
      JoinChord(nodeNumber);
    }
  }
  else if(command == "RINGSTATE")
  {
    //initiate ring state outputting
    CHORD_LOG("RingState<" << m_localHashNum << ">: Pred<" << m_addressNodeMap[predecessor.ip] << ", " << predecessor.hashValue << ">, Succ<" << m_addressNodeMap[successor.ip] << ", " << successor.hashValue << ">");
    SendRingstateReq(successor.ip, m_localNodeNum);
  }
  else if(command == "FIND")
  {
    iterator++;
    std::istringstream sin(*iterator);
    uint32_t chordKey;
    sin >> chordKey;
    LookupWrapper(GetNextTransactionId(), m_localNodeNum, chordKey, 0);
  }
  else if(command == "LEAVE") {
    NodeLeave();
  }
}

void
GUChord::CreateChord()
{
  //inside our own ring
  m_inChord = true;
  
  //set predecessor to null
  predecessor.ip = Ipv4Address::GetAny();
  
  //set successor to yourself
  successor.ip = m_local;
  successor.hashValue = hashIP(successor.ip);
}

void 
GUChord::JoinChord(uint32_t landmarkNode)
{
  //joined pre-existing ring
  m_inChord = true;

  //set predecessor and successor to NULL for now
  predecessor.ip = Ipv4Address::GetAny();
  successor.ip = Ipv4Address::GetAny();

  //ask for our successor
  SendSuccessorReq(m_nodeAddressMap[landmarkNode], m_localNodeNum, m_localHashNum);
}

uint32_t 
GUChord::hashIP(Ipv4Address ip)
{
  //SHA1
  uint32_t val = m_addressNodeMap[ip];
  unsigned char digest[SHA_DIGEST_LENGTH] = {0};

  //store input as char array in little endian order
  unsigned char ch[2];
  ch[0] = (val >> 8) & 0xff;
  ch[1] = (val) & 0xff;

  //hash last 16 on input only
  SHA1(ch, sizeof(uint16_t), (unsigned char*)&digest);

  uint64_t truncatedHash = 0;
  //7 chars is 56 bytes
  for(int i = 0; i < SHA_DIGEST_LENGTH; i++) {
      truncatedHash = (truncatedHash << 8) + int(digest[i]);
  }
  return truncatedHash % numRingSlots;
}

void
GUChord::SendPing (Ipv4Address destAddress, std::string pingMessage)
{
  if (destAddress != Ipv4Address::GetAny())
    {
      uint32_t transactionId = GetNextTransactionId ();
      CHORD_LOG ("Sending PING_REQ to Node: " << ReverseLookup(destAddress) << " IP: " << destAddress << " Message: " << pingMessage << " transactionId: " << transactionId);
      Ptr<PingRequest> pingRequest = Create<PingRequest> (transactionId, Simulator::Now(), destAddress, pingMessage);
      //Add to ping-tracker
      m_pingTracker.insert (std::make_pair (transactionId, pingRequest));
      Ptr<Packet> packet = Create<Packet> ();
      GUChordMessage message = GUChordMessage (GUChordMessage::PING_REQ, transactionId);
      message.SetPingReq (pingMessage);
      packet->AddHeader (message);
      m_socket->SendTo (packet, 0 , InetSocketAddress (destAddress, m_appPort));
    }
  else
    {
      // Report failure   
      m_pingFailureFn (destAddress, pingMessage);
    }
}

void
GUChord::RecvMessage (Ptr<Socket> socket)
{
  Address sourceAddr;
  Ptr<Packet> packet = socket->RecvFrom (sourceAddr);
  InetSocketAddress inetSocketAddr = InetSocketAddress::ConvertFrom (sourceAddr);
  Ipv4Address sourceAddress = inetSocketAddr.GetIpv4 ();
  uint16_t sourcePort = inetSocketAddr.GetPort ();
  GUChordMessage message;
  packet->RemoveHeader (message);

  switch (message.GetMessageType ())
    {
      case GUChordMessage::PING_REQ:
        ProcessPingReq (message, sourceAddress, sourcePort);
        break;
      case GUChordMessage::PING_RSP:
        ProcessPingRsp (message, sourceAddress, sourcePort);
        break;
      case GUChordMessage::SUCCESSOR_REQ:
        ProcessSuccessorReq(message, sourceAddress, sourcePort);
        break;
      case GUChordMessage::SUCCESSOR_RSP:
        ProcessSuccessorRsp(message, sourceAddress, sourcePort);
        break;
      case GUChordMessage::NOTIFY_SUCCESSOR:
        ProcessNotifySuccessor(message, sourceAddress, sourcePort);
        break;
      case GUChordMessage::NOTIFY_PREDECESSOR:
        ProcessNotifyPredecessor(message, sourceAddress, sourcePort);
        break;
      case GUChordMessage::GET_PREDECESSOR_OF_SUCCESSOR_REQ:
        ProcessPredecessorOfSuccReq(message, sourceAddress, sourcePort);
        break;
      case GUChordMessage::GET_PREDECESSOR_OF_SUCCESSOR_RSP:
        ProcessPredecessorOfSuccRsp(message, sourceAddress, sourcePort);
        break;
      case GUChordMessage::RINGSTATE_REQ:
        ProcessRingstateReq(message, sourceAddress, sourcePort);
        break;
      case GUChordMessage::LOOKUP_REQ:
        ProcessLookupReq(message, sourceAddress, sourcePort);
        break;
      case GUChordMessage::LOOKUP_RSP:
        ProcessLookupRsp(message, sourceAddress, sourcePort);
        break;
      case GUChordMessage::NODELEAVE:
        ProcessNodeLeave(message, sourceAddress, sourcePort);
        break;
      default:
        ERROR_LOG ("Unknown Message Type!");
        break;
    }
}

void
GUChord::ProcessPingReq (GUChordMessage message, Ipv4Address sourceAddress, uint16_t sourcePort)
{
    // Use reverse lookup for ease of debug
    std::string fromNode = ReverseLookup (sourceAddress);
    CHORD_LOG ("Received PING_REQ, From Node: " << fromNode << ", Message: " << message.GetPingReq().pingMessage);
    // Send Ping Response
    GUChordMessage resp = GUChordMessage (GUChordMessage::PING_RSP, message.GetTransactionId());
    resp.SetPingRsp (message.GetPingReq().pingMessage);
    Ptr<Packet> packet = Create<Packet> ();
    packet->AddHeader (resp);
    m_socket->SendTo (packet, 0 , InetSocketAddress (sourceAddress, sourcePort));
    // Send indication to application layer
    m_pingRecvFn (sourceAddress, message.GetPingReq().pingMessage);
}

void
GUChord::ProcessPingRsp (GUChordMessage message, Ipv4Address sourceAddress, uint16_t sourcePort)
{
  // Remove from pingTracker
  std::map<uint32_t, Ptr<PingRequest> >::iterator iter;
  iter = m_pingTracker.find (message.GetTransactionId ());
  if (iter != m_pingTracker.end ())
    {
      std::string fromNode = ReverseLookup (sourceAddress);
      CHORD_LOG ("Received PING_RSP, From Node: " << fromNode << ", Message: " << message.GetPingRsp().pingMessage);
      m_pingTracker.erase (iter);
      // Send indication to application layer
      m_pingSuccessFn (sourceAddress, message.GetPingRsp().pingMessage);
    }
  else
    {
      DEBUG_LOG ("Received invalid PING_RSP!");
    }
}

void
GUChord::AuditPings ()
{
  std::map<uint32_t, Ptr<PingRequest> >::iterator iter;
  for (iter = m_pingTracker.begin () ; iter != m_pingTracker.end();)
    {
      Ptr<PingRequest> pingRequest = iter->second;
      if (pingRequest->GetTimestamp().GetMilliSeconds() + m_pingTimeout.GetMilliSeconds() <= Simulator::Now().GetMilliSeconds())
        {
          DEBUG_LOG ("Ping expired. Message: " << pingRequest->GetPingMessage () << " Timestamp: " << pingRequest->GetTimestamp().GetMilliSeconds () << " CurrentTime: " << Simulator::Now().GetMilliSeconds ());
          // Remove stale entries
          m_pingTracker.erase (iter++);
          // Send indication to application layer
          m_pingFailureFn (pingRequest->GetDestinationAddress(), pingRequest->GetPingMessage ());
        }
      else
        {
          ++iter;
        }
    }
  //Rechedule timer
  m_auditPingsTimer.Schedule (m_pingTimeout); 
}

uint32_t
GUChord::GetNextTransactionId ()
{
  return m_currentTransactionId++;
}

void
GUChord::StopChord ()
{
  StopApplication ();
}

void
GUChord::SetPingSuccessCallback (Callback <void, Ipv4Address, std::string> pingSuccessFn)
{
  m_pingSuccessFn = pingSuccessFn;
}

void
GUChord::SetPingFailureCallback (Callback <void, Ipv4Address, std::string> pingFailureFn)
{
  m_pingFailureFn = pingFailureFn;
}

void
GUChord::SetPingRecvCallback (Callback <void, Ipv4Address, std::string> pingRecvFn)
{
  m_pingRecvFn = pingRecvFn;
}

void
GUChord::SetLookupCallback(Callback <void, Ipv4Address, uint32_t> lookup) {
  m_lookup = lookup;
}

void
GUChord::SetRedistributeKeysCallback (Callback <void,uint8_t, Ipv4Address, uint32_t> redistribute) {
  m_redistribute = redistribute;
}

void
GUChord::SendSuccessorReq (Ipv4Address destAddress, uint32_t originalNodeNum, uint32_t chordKey)
{
  //send a successor request asking for successor
  if (destAddress != Ipv4Address::GetAny() && destAddress != m_local)
    {
      uint32_t transactionId = GetNextTransactionId ();
      //CHORD_LOG ("Sending SUCCESSOR_REQ to Node: " << ReverseLookup(destAddress) << " IP: " << destAddress << " Original Node #: " << originalNodeNum << " Chord Key: " << chordKey << " transactionId: " << transactionId);
      Ptr<Packet> packet = Create<Packet> ();
      GUChordMessage message = GUChordMessage (GUChordMessage::SUCCESSOR_REQ, transactionId);
      message.SetSuccessorReq (originalNodeNum, chordKey);
      packet->AddHeader (message);
      m_socket->SendTo (packet, 0 , InetSocketAddress (destAddress, m_appPort));
    }
}

void 
GUChord::ProcessSuccessorReq(GUChordMessage message, Ipv4Address sourceAddress, uint16_t sourcePort)
{   
  //CHORD_LOG ("Received SUCCESSOR_REQ, From Node: " << ReverseLookup(sourceAddress) << ", Original Requestor: " << message.GetSuccessorReq().originalNodeNum << " Chord Key: " << message.GetSuccessorReq().chordKey);
    
  uint32_t originalNodeNum = message.GetSuccessorReq().originalNodeNum;
  uint32_t chordKey = message.GetSuccessorReq().chordKey;

  //if we have a successor
  if(successor.ip != Ipv4Address::GetAny())
    {
      //if requestor's chord index is between us and successor or if there is only one node in ring
      if(inOrder(m_localHashNum, chordKey, successor.hashValue) || (m_local == successor.ip))
        {
          //send original requestor a successorRsp with my successor as the payload
          SendSuccessorRsp(m_nodeAddressMap[originalNodeNum], m_addressNodeMap[successor.ip], successor.hashValue);

          //optional heuristic: update my successor to requestor
          successor.ip = m_nodeAddressMap[originalNodeNum];
          successor.hashValue = chordKey;
        }
      else
        {
          //CHORD_LOG ("Forwarding SUCCESSOR_REQ to Node: " << ReverseLookup(successor.ip) << " IP: " << successor.ip << " Original Node #: " << originalNodeNum << " Chord Key: " << chordKey << " transactionId: " << message.GetTransactionId());
          
          //send a successorReq with original message to my successor (replace to use finger table later)
          Ptr<Packet> packet = Create<Packet> ();
          packet->AddHeader(message);

          Ipv4Address destAddress = closestPrecedingNode(chordKey);

          m_socket->SendTo (packet, 0 , InetSocketAddress (destAddress, m_appPort));
        }
    }
}

void
GUChord::SendSuccessorRsp (Ipv4Address destAddress, uint32_t successorNodeNum, uint32_t successorChordKey)
{
  if (destAddress != Ipv4Address::GetAny() && destAddress != m_local)
    {
      uint32_t transactionId = GetNextTransactionId ();
      //CHORD_LOG ("Sending SUCCESSOR_RSP to Node: " << ReverseLookup(destAddress) << " IP: " << destAddress << " Successor Node #: " << successorNodeNum << " Chord Key: " << successorChordKey << " transactionId: " << transactionId);
      Ptr<Packet> packet = Create<Packet> ();
      GUChordMessage message = GUChordMessage (GUChordMessage::SUCCESSOR_RSP, transactionId);
      message.SetSuccessorRsp (successorNodeNum, successorChordKey);
      packet->AddHeader (message);
      m_socket->SendTo (packet, 0 , InetSocketAddress (destAddress, m_appPort));
    }
}

void 
GUChord::ProcessSuccessorRsp(GUChordMessage message, Ipv4Address sourceAddress, uint16_t sourcePort)
{
    //CHORD_LOG ("Received SUCCESSOR_RSP, From Node: " << ReverseLookup (sourceAddress) << ", New Successor: " << message.GetSuccessorRsp().successorNodeNum << " Successor Chord Key: " << message.GetSuccessorRsp().successorChordKey);
    
    uint32_t successorNodeNum = message.GetSuccessorRsp().successorNodeNum;
    uint32_t chordKey = message.GetSuccessorRsp().successorChordKey;

    //update successor
    successor.ip = m_nodeAddressMap[successorNodeNum];
    successor.hashValue = chordKey;

    //notify new successor
    NotifySuccessor(successor.ip);

    //update predecessor
    predecessor.ip = sourceAddress;
    predecessor.hashValue = hashIP(sourceAddress);

    //optional heuristic: notify predecessor
    NotifyPredecessor(predecessor.ip);
}

void 
GUChord::NotifySuccessor(Ipv4Address destAddress)
{
  //watch out for the case where a single node in a ring sends to itself
  if (destAddress != Ipv4Address::GetAny () && destAddress != m_local)
    {
      uint32_t transactionId = GetNextTransactionId ();
      //CHORD_LOG ("Sending NOTIFY_SUCCESSOR to Node: " << ReverseLookup(destAddress) << " IP: " << destAddress << " transactionId: " << transactionId);
      Ptr<Packet> packet = Create<Packet> ();
      GUChordMessage message = GUChordMessage (GUChordMessage::NOTIFY_SUCCESSOR, transactionId);
      packet->AddHeader (message);
      m_socket->SendTo (packet, 0 , InetSocketAddress (destAddress, m_appPort));
    }
}

void 
GUChord::ProcessNotifySuccessor(GUChordMessage message, Ipv4Address sourceAddress, uint16_t sourcePort)
{
    //CHORD_LOG ("Received NOTIFY_SUCCESSOR, From Node: " << ReverseLookup (sourceAddress));
    
    //could be a new predecessor if it's closer
    uint32_t possiblePredHashIndex = hashIP(sourceAddress);

    //if current predecessor == null or possible predecessor is between current predecessor and node
    if(predecessor.ip == Ipv4Address::GetAny() || inOrder(predecessor.hashValue, possiblePredHashIndex, m_localHashNum))
      {
        //update predecessor
        predecessor.ip = sourceAddress;
        predecessor.hashValue = possiblePredHashIndex;

        m_redistribute(0xff,predecessor.ip, predecessor.hashValue);
      }
}

bool
GUChord::isAlone() {
  uint32_t alone = 0;
  return (successor.ip == m_local || successor.ip == Ipv4Address(alone));
}

void 
GUChord::NotifyPredecessor(Ipv4Address destAddress)
{
  if (destAddress != Ipv4Address::GetAny() && destAddress != m_local)
    {
      uint32_t transactionId = GetNextTransactionId ();
      //CHORD_LOG ("Sending NOTIFY_PREDECESSOR to Node: " << ReverseLookup(destAddress) << " IP: " << destAddress << " transactionId: " << transactionId);
      Ptr<Packet> packet = Create<Packet> ();
      GUChordMessage message = GUChordMessage (GUChordMessage::NOTIFY_PREDECESSOR, transactionId);
      packet->AddHeader (message);
      m_socket->SendTo (packet, 0 , InetSocketAddress (destAddress, m_appPort));
    }
}

void
GUChord::ProcessNotifyPredecessor(GUChordMessage message, Ipv4Address sourceAddress, uint16_t sourcePort)
{
  //CHORD_LOG ("Received NOTIFY_PREDECESSOR, From Node: " << ReverseLookup (sourceAddress));
    
  //a possibly new successor if it is closer to us than the current successor
  uint32_t possibleSuccHashIndex = hashIP(sourceAddress);

  //if current successor == null or possible successor is between node and current successor
  if(successor.ip == Ipv4Address::GetAny() || inOrder(m_localHashNum, possibleSuccHashIndex, successor.hashValue))
    {
      //update successor
      successor.ip = sourceAddress;
      successor.hashValue = possibleSuccHashIndex;
    }
}

void 
GUChord::SendPredecessorOfSuccReq(Ipv4Address destAddress)
{
  //ask successor for predecessor
  if (destAddress != Ipv4Address::GetAny() && destAddress != m_local)
    {
      uint32_t transactionId = GetNextTransactionId ();
      //CHORD_LOG ("Sending GET_PREDECESSOR_OF_SUCCESSOR_REQ to Node: " << ReverseLookup(destAddress) << " IP: " << destAddress << " transactionId: " << transactionId);
      Ptr<Packet> packet = Create<Packet> ();
      GUChordMessage message = GUChordMessage (GUChordMessage::GET_PREDECESSOR_OF_SUCCESSOR_REQ, transactionId);
      packet->AddHeader (message);
      m_socket->SendTo (packet, 0 , InetSocketAddress (destAddress, m_appPort));
    }
}

void 
GUChord::ProcessPredecessorOfSuccReq(GUChordMessage message, Ipv4Address sourceAddress, uint16_t sourcePort)
{
  //CHORD_LOG ("Received PREDECESSOR_OF_SUCCESSOR_REQ, From Node: " << ReverseLookup (sourceAddress));
  
  //send predecessor in reply
  SendPredecessorOfSuccRsp(sourceAddress, m_addressNodeMap[predecessor.ip], predecessor.hashValue);
}

void 
GUChord::SendPredecessorOfSuccRsp(Ipv4Address destAddress, uint32_t predecessorNodeNum, uint32_t predecessorChordKey)
{
  //send current predecessor
  if (destAddress != Ipv4Address::GetAny() && destAddress != m_local)
    {
      uint32_t transactionId = GetNextTransactionId ();
      //CHORD_LOG ("Sending PREDECESSOR_OF_SUCCESSOR_RSP to Node: " << ReverseLookup(destAddress) << " IP: " << destAddress << " Predecessor Node #: " << predecessorNodeNum << " Predecessor Chord Key: " << predecessorChordKey << " transactionId: " << transactionId);
      Ptr<Packet> packet = Create<Packet> ();
      GUChordMessage message = GUChordMessage (GUChordMessage::GET_PREDECESSOR_OF_SUCCESSOR_RSP, transactionId);
      message.SetPredecessorOfSuccessorRsp (predecessorNodeNum, predecessorChordKey);
      packet->AddHeader (message);
      m_socket->SendTo (packet, 0 , InetSocketAddress (destAddress, m_appPort));
    }
}
    
void
GUChord::ProcessPredecessorOfSuccRsp(GUChordMessage message, Ipv4Address sourceAddress, uint16_t sourcePort)
{
  //CHORD_LOG ("Received PREDECESSOR_OF_SUCCESSOR_RSP, From Node: " << ReverseLookup (sourceAddress) << ", Predecessor: " << message.GetPredecessorOfSuccessorRsp().predecessorNodeNum << " Predecessor Chord Key: " << message.GetPredecessorOfSuccessorRsp().predecessorChordKey);
    
  uint32_t predecessorNodeNum =  message.GetPredecessorOfSuccessorRsp().predecessorNodeNum;
  uint32_t predecessorChordKey =  message.GetPredecessorOfSuccessorRsp().predecessorChordKey;

  //if currently no successor or successor's predecessor is between current node and supposed "successor"
  if(successor.ip == Ipv4Address::GetAny() || inOrder(m_localHashNum, predecessorChordKey, successor.hashValue))
    {
      //update successor
      successor.ip = m_nodeAddressMap[predecessorNodeNum];
      successor.hashValue = predecessorChordKey;
    }

  //optional heuristic: notify possibly new successor
  NotifySuccessor(successor.ip);
}

void 
GUChord::SendRingstateReq(Ipv4Address destAddress, uint32_t originalNodeNum)
{
  //send ring state request (to successor)
  if (destAddress != Ipv4Address::GetAny() && destAddress != m_local)
    {
      uint32_t transactionId = GetNextTransactionId ();
      //CHORD_LOG ("Sending RINGSTATE_REQ to Node: " << ReverseLookup(destAddress) << " IP: " << destAddress << " Original Node #: " << originalNodeNum << " transactionId: " << transactionId);
      Ptr<Packet> packet = Create<Packet> ();
      GUChordMessage message = GUChordMessage (GUChordMessage::RINGSTATE_REQ, transactionId);
      message.SetRingstateReq (originalNodeNum);
      packet->AddHeader (message);
      m_socket->SendTo (packet, 0 , InetSocketAddress (destAddress, m_appPort));
    }
}

void 
GUChord::ProcessRingstateReq(GUChordMessage message, Ipv4Address sourceAddress, uint16_t sourcePort)
{
  //CHORD_LOG ("Received RINGSTATE_REQ, From Node: " << ReverseLookup (sourceAddress) << ", Original Requestor: " << message.GetRingstateReq().originalNodeNum);
    
  uint32_t originalNodeNum = message.GetRingstateReq().originalNodeNum;
  if(originalNodeNum != m_addressNodeMap[m_local])
    {
      //print ring state output for this node
      CHORD_LOG("RingState<" << m_localHashNum << ">: Pred<" << m_addressNodeMap[predecessor.ip] << ", " << predecessor.hashValue << ">, Succ<" << m_addressNodeMap[successor.ip] << ", " << successor.hashValue << ">");
      //forward because we did not initiate this request
      SendRingstateReq(successor.ip, originalNodeNum);
    }
}

void
GUChord::NodeLeave() {
  CHORD_LOG("NOW LEAVING");
  SendNodeLeaveMsg(0); //ispredecessor
  SendNodeLeaveMsg(0xFF); //issuccessor

  //CALLBACK: reuse redistribute, first 0x0 is to say that this is for node leave
  //second 0x0 is just a dummy value, this doesn't matter for nodeleave so anything works
  m_redistribute(0x0, successor.ip, 0x0);

  //reset successor and predecessor
  successor.ip = Ipv4Address::GetAny();
  successor.hashValue = numRingSlots + 1;
  predecessor.ip = Ipv4Address::GetAny();
  predecessor.hashValue = numRingSlots + 1;

  //reset finger table
  fingerTable = std::vector<hashNode> (fingerTableSize);
  for(uint32_t tableIndex = 0; tableIndex < fingerTableSize; tableIndex++)
  {
    fingerTable[tableIndex].ip = Ipv4Address::GetAny();
    fingerTable[tableIndex].hashValue = numRingSlots + 1;
  }
}

void
GUChord::SendNodeLeaveMsg(uint8_t isSuccessor) {
  Ipv4Address destAddress;
  uint32_t hashedNode;
  uint32_t ipAddr;
  if(isSuccessor) {
    destAddress = Ipv4Address(predecessor.ip);
    ipAddr = successor.ip.Get();
    hashedNode = successor.hashValue;
  }
  else {
    destAddress = Ipv4Address(successor.ip);
    ipAddr = predecessor.ip.Get();
    hashedNode = predecessor.hashValue;
  }
  //send ring state request (to successor)
  if (destAddress != Ipv4Address::GetAny() && destAddress != m_local)
    {
      uint32_t transactionId = GetNextTransactionId ();
      CHORD_LOG ("Sending NODELEAVE to Node: " << ReverseLookup(destAddress) << " IP: " << destAddress);
      Ptr<Packet> packet = Create<Packet> ();
      GUChordMessage message = GUChordMessage (GUChordMessage::NODELEAVE, transactionId);
      message.SetNodeLeave(isSuccessor, hashedNode, ipAddr);
      packet->AddHeader (message);
      m_socket->SendTo (packet, 0 , InetSocketAddress (destAddress, m_appPort));
    }
}

void
GUChord::ProcessNodeLeave(GUChordMessage message, Ipv4Address sourceAddress, uint16_t sourcePort) {
  CHORD_LOG("received node leave");
  uint8_t isSuccessor = message.GetNodeLeave().isSuccessor;
  if(isSuccessor) {
    successor.ip = Ipv4Address(message.GetNodeLeave().ipAddr);
    successor.hashValue = message.GetNodeLeave().hashedNode;
  }
  else {
    predecessor.ip = Ipv4Address(message.GetNodeLeave().ipAddr);
    predecessor.hashValue = message.GetNodeLeave().hashedNode;
  }
}

//returns true if b is between a and c (clockwise) on ring, false otherwise
bool
GUChord::inOrder(uint32_t a, uint32_t b, uint32_t c)
{
  uint32_t shift = numRingSlots - a;
  a = 0;
  b = (b + shift) % numRingSlots;
  c = (c + shift) % numRingSlots;
  
  if(a < b && b <= c) //<= used because of (n, successor]
    {
      return true;
    }
  return false;
}

//check if we own the key
bool
GUChord::isOwn(uint32_t hashedKey) {
  return inOrder(predecessor.hashValue, hashedKey, m_localHashNum);
}

//check that predecessor of successor is ourselves (not in between, otherwise update successor)
void
GUChord::stabilize()
{
  SendPredecessorOfSuccReq(successor.ip);
  
  //schedule stabilization for future
  m_stabilizeTimer.Schedule (m_stabilizeTimeOut); 
}

void
GUChord::PrintFingerTable()
{
  std::cout << "---------- Start of Finger Table ----------" << std::endl;
  for(uint32_t tableIndex = 0; tableIndex < fingerTableSize; tableIndex++)
  {
    std::cout << "m = " << tableIndex << " 2^m = " << pow(2, tableIndex) << " 2^m + current = " << (uint32_t)(m_localHashNum + pow(2, tableIndex)) % numRingSlots << " Ip: " << fingerTable[tableIndex].ip << " Node #: " << m_addressNodeMap[fingerTable[tableIndex].ip] << " Hash: " << fingerTable[tableIndex].hashValue << std::endl;
  }
  std::cout << "---------- End of Finger Table ----------" << std::endl;
}

void
GUChord::FixFingerTable()
{
  uint32_t tableIndex = 0;
  //if we have a successor
  if(successor.ip != Ipv4Address::GetAny())
  {
    //populate as much of the table with the successor as possible
    while((tableIndex < fingerTableSize) && inOrder(m_localHashNum, ((uint32_t)(m_localHashNum + pow(2, tableIndex)) % numRingSlots), successor.hashValue))
    {
      fingerTable[tableIndex] = successor;
      tableIndex++;
    }
  }

  //heuristic
  uint32_t topOfTable = fingerTableSize - 1;
  hashNode local;
  local.ip = m_local;
  local.hashValue = m_localHashNum;
  //if we have a predecessor
  if(predecessor.ip != Ipv4Address::GetAny())
  {
    while(topOfTable >= tableIndex && inOrder(predecessor.hashValue, ((uint32_t)(m_localHashNum + pow(2, topOfTable)) % numRingSlots), m_localHashNum))
    {
      fingerTable[topOfTable] = local;
      topOfTable--;
    }
  }
	
  //don't message yourself for finger fixing, don't finger fix if only one node in ring
  if(successor.ip != Ipv4Address::GetAny() && successor.ip != m_local)
  {
    while(tableIndex <= topOfTable)
    {
      uint32_t transactionNum = GetNextTransactionId();
      fingerRequestTracker[transactionNum] = tableIndex;
      uint32_t lookupKey = (uint32_t)(m_localHashNum + pow(2, tableIndex)) % numRingSlots;
      SendLookupReq(transactionNum, m_localNodeNum, lookupKey, 1); //fix fingers is within chord
      tableIndex++;
    }
  }

  //schedule finger fixing for future
  m_fixFingersTimer.Schedule (m_fixFingersTimeOut);
}

void
GUChord::SendLookupReq (uint32_t transactionNum, uint32_t originalNodeNum, uint32_t lookupKey, uint8_t fromChord)
{
  Ipv4Address destAddress = closestPrecedingNode(lookupKey);

  //send a lookup request
  if (destAddress != Ipv4Address::GetAny() && destAddress != m_local)
    {
      //CHORD_LOG ("Sending LOOKUP_REQ to Node: " << ReverseLookup(destAddress) << " IP: " << destAddress << " Original Node #: " << originalNodeNum << " Lookup Key: " << lookupKey << " transactionId: " << transactionNum);
      Ptr<Packet> packet = Create<Packet> ();
      GUChordMessage message = GUChordMessage (GUChordMessage::LOOKUP_REQ, transactionNum);
      message.SetLookupReq (originalNodeNum, lookupKey, fromChord);
      packet->AddHeader (message);
      m_socket->SendTo (packet, 0 , InetSocketAddress (destAddress, m_appPort));
    }
}

//returns ip of closest preceding node from finger table
Ipv4Address
GUChord::closestPrecedingNode(uint32_t targetKey)
{
  Ipv4Address destAddress = successor.ip;
  int index = (int)(fingerTableSize - 1);
  bool found = false;
  hashNode tmp;
  while(index >= 0 && !found)
  {
    tmp = fingerTable[index];
    if(tmp.ip != Ipv4Address::GetAny() && inBetween(m_localHashNum, tmp.hashValue, targetKey))
    {
      found = true;
      destAddress = tmp.ip;
    }
    index--;
  }
  return destAddress;
}

//returns true if b is between a and c (clockwise) on ring, false otherwise
bool
GUChord::inBetween(uint32_t a, uint32_t b, uint32_t c)
{
  uint32_t shift = numRingSlots - a;
  a = 0;
  b = (b + shift) % numRingSlots;
  c = (c + shift) % numRingSlots;

  if(a < b && b < c)
    {
      return true;
    }
  return false;
}

void 
GUChord::ProcessLookupReq(GUChordMessage message, Ipv4Address sourceAddress, uint16_t sourcePort)
{
  //CHORD_LOG ("Received LOOKUP_REQ, From Node: " << ReverseLookup(sourceAddress) << ", Original Requestor: " << message.GetLookupReq().originalNodeNum << " Lookup Key: " << message.GetLookupReq().lookupKey);
    
  uint32_t originalNodeNum = message.GetLookupReq().originalNodeNum;
  uint32_t lookupKey = message.GetLookupReq().lookupKey;
  uint8_t fromChord = message.GetLookupReq().fromChord;

  if((successor.ip != Ipv4Address::GetAny()) && inOrder(m_localHashNum, lookupKey, successor.hashValue))
  {
    //send response that our successor has it
    SendLookupRsp(m_nodeAddressMap[originalNodeNum], message.GetTransactionId(), m_addressNodeMap[successor.ip], lookupKey, fromChord);
  }
  else if((predecessor.ip != Ipv4Address::GetAny()) && inOrder(predecessor.hashValue, lookupKey, m_localHashNum))
  {
    //send response that we have it
    SendLookupRsp(m_nodeAddressMap[originalNodeNum], message.GetTransactionId(), m_localNodeNum, lookupKey, fromChord);
  }
  else
  {
    //forward this request to next node based on finger table
    if(fromChord == 0)
    {
      Ipv4Address dest = closestPrecedingNode(lookupKey);
      CHORD_LOG("LookupRequest<" << m_localHashNum << ">:NextHop<" << dest << ", " << hashIP(dest) << ", " << lookupKey << ">");
    }
    SendLookupReq(message.GetTransactionId(), originalNodeNum, lookupKey, fromChord);
  }
}
    
void 
GUChord::SendLookupRsp(Ipv4Address destAddress, uint32_t transactionNum, uint32_t successorNodeNum, uint32_t lookupKey, uint8_t fromChord)
{
  //LookupResult<currentNodeKey, targetKey, originatorNode>
  if(fromChord == 0)
  {
    CHORD_LOG("LookupResult<" << hashIP(m_nodeAddressMap[successorNodeNum]) << ", " << lookupKey << ", " << m_addressNodeMap[destAddress] << ">");
  }

  if (destAddress != Ipv4Address::GetAny() && destAddress != m_local)
    {
      //CHORD_LOG ("Sending LOOKUP_RSP to Node: " << ReverseLookup(destAddress) << " IP: " << destAddress << " Successor Node #: " << successorNodeNum << " Lookup Key: " << lookupKey << " transactionId: " << transactionNum);
      Ptr<Packet> packet = Create<Packet> ();
      GUChordMessage message = GUChordMessage (GUChordMessage::LOOKUP_RSP, transactionNum);
      message.SetLookupRsp (successorNodeNum, lookupKey, fromChord);
      packet->AddHeader (message);
      m_socket->SendTo (packet, 0 , InetSocketAddress (destAddress, m_appPort));
    }
}
    
void
GUChord::ProcessLookupRsp(GUChordMessage message, Ipv4Address sourceAddress, uint16_t sourcePort)
{
    //CHORD_LOG ("Received LOOKUP_RSP, From Node: " << ReverseLookup(sourceAddress) << ", Successor: " << message.GetLookupRsp().successorNodeNum << " Lookup Key: " << message.GetLookupRsp().lookupKey << " transactionId: " << message.GetTransactionId());

    uint32_t successorNodeNum = message.GetLookupRsp().successorNodeNum;
    uint32_t lookupKey = message.GetLookupRsp().lookupKey;
    uint32_t transactionNum = message.GetTransactionId();

    //check if its a finger table entry
    std::map<uint32_t, uint32_t>::iterator it = fingerRequestTracker.find(transactionNum);
    if(it != fingerRequestTracker.end())
    {


      if(!inOrder(m_localHashNum, lookupKey, hashIP(m_nodeAddressMap[successorNodeNum])))
      {
        CHORD_LOG("WARNING: ABOUT TO ADD A BROKEN ENTRY TO FINGER TABLE");
        CHORD_LOG(m_localHashNum << "<" << lookupKey << "<" << hashIP(m_nodeAddressMap[successorNodeNum]));
      }

      //found lookup request in finger table requests

      //if so then add to finger table, remove from finger table tracker
      uint32_t tableIndex = fingerRequestTracker[transactionNum];
      hashNode tmp;
      tmp.ip = m_nodeAddressMap[successorNodeNum];
      tmp.hashValue = hashIP(tmp.ip);
      fingerTable[tableIndex] = tmp;

      //remove from finger table tracker
      fingerRequestTracker.erase(it);
    }
    else
    {
      //if not then do the callback
      m_lookup(m_nodeAddressMap[successorNodeNum], transactionNum);
    }
}
 
void 
GUChord::LookupWrapper(uint32_t transactionNum, uint32_t originalNodeNum, uint32_t lookupKey, uint8_t fromChord)
{
  if(fromChord == 0)
  {  
    CHORD_LOG("LookupIssue<" << m_localHashNum << ", " << lookupKey << ">")
  }
  SendLookupReq(transactionNum, originalNodeNum, lookupKey, fromChord);
}
    
void 
GUChord::GUSearchLookup(uint32_t transactionNum, uint32_t lookupKey) {
    LookupWrapper(transactionNum, m_localNodeNum, lookupKey, 0);
}