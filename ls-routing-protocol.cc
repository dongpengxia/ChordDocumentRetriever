#include "ns3/inet-socket-address.h"
#include "ns3/ipv4-header.h"
#include "ns3/ipv4-route.h"
#include "ns3/log.h"
#include "ns3/ls-routing-protocol.h"
#include "ns3/random-variable.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/test-result.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/uinteger.h"

#include <sys/time.h>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LSRoutingProtocol");
NS_OBJECT_ENSURE_REGISTERED (LSRoutingProtocol);

TypeId
LSRoutingProtocol::GetTypeId (void)
{
  static TypeId tid = TypeId ("LSRoutingProtocol")
  .SetParent<GURoutingProtocol> ()
  .AddConstructor<LSRoutingProtocol> ()
  .AddAttribute ("LSPort",
                 "Listening port for LS packets",
                 UintegerValue (5000),
                 MakeUintegerAccessor (&LSRoutingProtocol::m_lsPort),
                 MakeUintegerChecker<uint16_t> ())
  .AddAttribute ("PingTimeout",
                 "Timeout value for PING_REQ in milliseconds",
                 TimeValue (MilliSeconds (2000)),
                 MakeTimeAccessor (&LSRoutingProtocol::m_pingTimeout),
                 MakeTimeChecker ())
  .AddAttribute ("MaxTTL",
                 "Maximum TTL value for LS packets",
                 UintegerValue (16),
                 MakeUintegerAccessor (&LSRoutingProtocol::m_maxTTL),
                 MakeUintegerChecker<uint8_t> ())

  //Hello Period is the amount of time between each iteration of sending out neighbor request packets
  .AddAttribute ("HelloPeriod",
                 "Hello Period in milliseconds",
                 TimeValue (MilliSeconds (30000)), //use 1000000000 if you want to be able to read log messages without overflowing the screen
                 MakeTimeAccessor (&LSRoutingProtocol::m_helloPeriod),
                 MakeTimeChecker ())
  //Neighbor Timeout is the amount of time that an entry in the neighbor table lasts before it is invalid
  .AddAttribute ("NeighborTimeout",
                 "Neighbor Timeout in milliseconds",
                 TimeValue (MilliSeconds (91000)), //use 1900000000 if you want to be able to read log messages without overflowing the screen
                 MakeTimeAccessor (&LSRoutingProtocol::m_neighborTimeout),
                 MakeTimeChecker ())
  ;

  return tid;
}

LSRoutingProtocol::LSRoutingProtocol ()
  : m_auditPingsTimer (Timer::CANCEL_ON_DESTROY)
{
  RandomVariable random;
  SeedManager::SetSeed (time (NULL));
  random = UniformVariable (0x00000000, 0xFFFFFFFF);
  m_currentSequenceNumber = random.GetInteger ();
  // Setup static routing 
  m_staticRouting = Create<Ipv4StaticRouting> ();
}

LSRoutingProtocol::~LSRoutingProtocol () {}

void 
LSRoutingProtocol::DoDispose ()
{
  // Close sockets
  for (std::map< Ptr<Socket>, Ipv4InterfaceAddress >::iterator iter = m_socketAddresses.begin ();
       iter != m_socketAddresses.end (); iter++)
    {
      iter->first->Close ();
    }
  m_socketAddresses.clear ();
  
  // Clear static routing
  m_staticRouting = 0;

  // Cancel timers
  m_auditPingsTimer.Cancel ();
 
  m_pingTracker.clear (); 

  //Clear neighbor table
  neighborTable.clear();

  //Clear relationship table
  relationshipTable.clear();

  //Clear routing table
  routingTable.clear();

  GURoutingProtocol::DoDispose ();
}

void
LSRoutingProtocol::SetMainInterface (uint32_t mainInterface)
{
  m_mainAddress = m_ipv4->GetAddress (mainInterface, 0).GetLocal ();
}

void
LSRoutingProtocol::SetNodeAddressMap (std::map<uint32_t, Ipv4Address> nodeAddressMap)
{
  m_nodeAddressMap = nodeAddressMap;
}

void
LSRoutingProtocol::SetAddressNodeMap (std::map<Ipv4Address, uint32_t> addressNodeMap)
{
  m_addressNodeMap = addressNodeMap;
}

Ipv4Address
LSRoutingProtocol::ResolveNodeIpAddress (uint32_t nodeNumber)
{
  std::map<uint32_t, Ipv4Address>::iterator iter = m_nodeAddressMap.find (nodeNumber);
  if (iter != m_nodeAddressMap.end ())
    { 
      return iter->second;
    }
  return Ipv4Address::GetAny ();
}

std::string
LSRoutingProtocol::ReverseLookup (Ipv4Address ipAddress)
{
  std::map<Ipv4Address, uint32_t>::iterator iter = m_addressNodeMap.find (ipAddress);
  if (iter != m_addressNodeMap.end ())
    { 
      std::ostringstream sin;
      uint32_t nodeNumber = iter->second;
      sin << nodeNumber;    
      return sin.str();
    }
  return "Unknown";
}

void
LSRoutingProtocol::DoStart ()
{
  // Create sockets
  for (uint32_t i = 0 ; i < m_ipv4->GetNInterfaces () ; i++)
    {
      Ipv4Address ipAddress = m_ipv4->GetAddress (i, 0).GetLocal ();
      if (ipAddress == Ipv4Address::GetLoopback ())
        continue;
      // Create socket on this interface
      Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (),
          UdpSocketFactory::GetTypeId ());
      socket->SetAllowBroadcast (true);
      InetSocketAddress inetAddr (m_ipv4->GetAddress (i, 0).GetLocal (), m_lsPort);
      socket->SetRecvCallback (MakeCallback (&LSRoutingProtocol::RecvLSMessage, this));
      if (socket->Bind (inetAddr))
        {
          NS_FATAL_ERROR ("LSRoutingProtocol::DoStart::Failed to bind socket!");
        }
      Ptr<NetDevice> netDevice = m_ipv4->GetNetDevice (i);
      socket->BindToNetDevice (netDevice);
      m_socketAddresses[socket] = m_ipv4->GetAddress (i, 0);
    }
  // Configure timers
  m_auditPingsTimer.SetFunction (&LSRoutingProtocol::AuditPings, this);

  // Start timers
  m_auditPingsTimer.Schedule (m_pingTimeout);

  //send "HELLO" message to all directly connected nodes
  Simulator::ScheduleNow (&LSRoutingProtocol::SendHello, this);

  //begin periodic auditing of neighbor table to delete outdated entries
  Simulator::ScheduleNow (&LSRoutingProtocol::auditNeighborTable, this);
}

Ptr<Ipv4Route>
LSRoutingProtocol::RouteOutput (Ptr<Packet> packet, const Ipv4Header &header, Ptr<NetDevice> outInterface, Socket::SocketErrno &sockerr)
{
  Ptr<Ipv4Route> rtentry;
  Ipv4Address destAddr = header.GetDestination();
  std::map<uint32_t, routingTableEntry>::iterator search = routingTable.find(atoi(ReverseLookup(destAddr).c_str()));
  if(search != routingTable.end())
    {
      //if destination is in routing table
      rtentry = Create<Ipv4Route>();
      rtentry->SetDestination(destAddr);
      Ipv4InterfaceAddress ifAddr = search->second.interfaceAddress;
      rtentry->SetSource (ifAddr.GetLocal ());
      Ipv4Address nextHopAddr = search->second.nextHopAddress;
      rtentry->SetGateway(nextHopAddr);
      int32_t interfaceIndex = m_ipv4->GetInterfaceForAddress(ifAddr.GetLocal());
      rtentry->SetOutputDevice (m_ipv4->GetNetDevice (interfaceIndex));
      sockerr = Socket::ERROR_NOTERROR;
      
      TRAFFIC_LOG("LS node " << m_mainAddress << ": RouteOutput for dest=" << destAddr << " --> nextHop=" << nextHopAddr << " interface=" << ifAddr.GetLocal());
      DEBUG_LOG("Found route to " << rtentry->GetDestination() << " via nh " << rtentry->GetGateway() << " with source addr " << rtentry->GetSource() << " and output dev " << rtentry->GetOutputDevice());
    }
  else
    {
      DEBUG_LOG("LS node " << m_mainAddress << ": RouteOutput for dest=" << header.GetDestination() << " No route to host");
      sockerr = Socket::ERROR_NOROUTETOHOST;
    }
  return rtentry;
}

bool 
LSRoutingProtocol::RouteInput  (Ptr<const Packet> packet, 
  const Ipv4Header &header, Ptr<const NetDevice> inputDev,                            
  UnicastForwardCallback ucb, MulticastForwardCallback mcb,             
  LocalDeliverCallback lcb, ErrorCallback ecb)
{
  Ipv4Address destinationAddress = header.GetDestination ();
  Ipv4Address sourceAddress = header.GetSource ();

  // Drop if packet was originated by this node
  if (IsOwnAddress (sourceAddress) == true)
    {
      return true;
    }

  // Check for local delivery
  uint32_t interfaceNum = m_ipv4->GetInterfaceForDevice (inputDev);
  if (m_ipv4->IsDestinationAddress (destinationAddress, interfaceNum))
    {
      if (!lcb.IsNull ())
        {
          TRAFFIC_LOG("Local delivery to " << destinationAddress);
          lcb (packet, header, interfaceNum);
          return true;
        }
      else
        {
          return false;
        }
    }

  //Forwarding
  Ptr<Ipv4Route> rtentry;
  Ipv4Address destAddr = header.GetDestination();
  std::map<uint32_t, routingTableEntry>::iterator search = routingTable.find(atoi(ReverseLookup(destAddr).c_str()));
  if(search != routingTable.end())
    {
      //if destination is in routing 
      rtentry = Create<Ipv4Route>();
      rtentry->SetDestination(destAddr);
      Ipv4InterfaceAddress ifAddr = search->second.interfaceAddress;
      rtentry->SetSource (ifAddr.GetLocal ());
      Ipv4Address nextHopAddr = search->second.nextHopAddress;
      rtentry->SetGateway(nextHopAddr);
      int32_t interfaceIndex = m_ipv4->GetInterfaceForAddress(ifAddr.GetLocal());
      rtentry->SetOutputDevice (m_ipv4->GetNetDevice (interfaceIndex));
      
      TRAFFIC_LOG("LS node " << m_mainAddress << ": RouteInput for dest=" << destAddr << " --> nextHop=" << nextHopAddr << " interface=" << ifAddr.GetLocal());
      ucb(rtentry, packet, header);
      return true;  
    }
  else
    {
      DEBUG_LOG("LS node " << m_mainAddress << ": RouteInput for dest=" << header.GetDestination() << " No route to host");
    }
  return false;
}

void
LSRoutingProtocol::BroadcastPacket (Ptr<Packet> packet)
{
  for (std::map<Ptr<Socket> , Ipv4InterfaceAddress>::const_iterator i =
      m_socketAddresses.begin (); i != m_socketAddresses.end (); i++)
    {
      Ipv4Address broadcastAddr = i->second.GetLocal ().GetSubnetDirectedBroadcast (i->second.GetMask ());
      i->first->SendTo (packet, 0, InetSocketAddress (broadcastAddr, m_lsPort));
    }
}

void
LSRoutingProtocol::ProcessCommand (std::vector<std::string> tokens)
{
  std::vector<std::string>::iterator iterator = tokens.begin();
  std::string command = *iterator;
  if (command == "PING")
    {
      if (tokens.size() < 3)
        {
          ERROR_LOG ("Insufficient PING params..."); 
          return;
        }
      iterator++;
      std::istringstream sin (*iterator);
      uint32_t nodeNumber;
      sin >> nodeNumber;
      iterator++;
      std::string pingMessage = *iterator;
      Ipv4Address destAddress = ResolveNodeIpAddress (nodeNumber);
      if (destAddress != Ipv4Address::GetAny ())
        {
          uint32_t sequenceNumber = GetNextSequenceNumber ();
          TRAFFIC_LOG ("Sending PING_REQ to Node: " << nodeNumber << " IP: " << destAddress << " Message: " << pingMessage << " SequenceNumber: " << sequenceNumber);
          Ptr<PingRequest> pingRequest = Create<PingRequest> (sequenceNumber, Simulator::Now(), destAddress, pingMessage);
          // Add to ping-tracker
          m_pingTracker.insert (std::make_pair (sequenceNumber, pingRequest));
          Ptr<Packet> packet = Create<Packet> ();
          LSMessage lsMessage = LSMessage (LSMessage::PING_REQ, sequenceNumber, m_maxTTL, m_mainAddress);
          lsMessage.SetPingReq (destAddress, pingMessage);
          packet->AddHeader (lsMessage);
          BroadcastPacket (packet);
        }
    }
  else if (command == "DUMP")
    {
      if (tokens.size() < 2)
        {
          ERROR_LOG ("Insufficient Parameters!");
          return;
        }
      iterator++;
      std::string table = *iterator;
      if (table == "ROUTES" || table == "ROUTING")
        {
          DumpRoutingTable ();
        }
      else if (table == "NEIGHBORS" || table == "NEIGHBOURS")
        {
          DumpNeighbors ();
        }
      else if (table == "LSA")
        {
          DumpLSA ();
        }
    }
}

void
LSRoutingProtocol::DumpLSA ()
{
  STATUS_LOG (std::endl << "**************** LSA DUMP ********************" << std::endl
              << "Node\t\tNeighbor(s)");
  PRINT_LOG ("");
}

void
LSRoutingProtocol::DumpNeighbors ()
{
  STATUS_LOG (std::endl << "**************** Neighbor List ********************" << std::endl
              << "NeighborNumber\t\tNeighborAddr\t\tInterfaceAddr");

  auditNeighborTable();

  //print neighbor table
  for(std::map<uint32_t, neighborEntryValue>::const_iterator it = neighborTable.begin(); it != neighborTable.end(); it++)
    {
      PRINT_LOG(std::setw(14) << it->first << "\t\t" << std::setw(6) << it->second.neighborIPAddress << "\t\t" 
                 << std::setw(6) << it->second.interfaceIPAddress.GetLocal());
    }
}

void
LSRoutingProtocol::DumpRoutingTable ()
{
	STATUS_LOG (std::endl << "**************** Route Table ********************" << std::endl
			  << "DestNumber\t\tDestAddr\t\tNextHopNumber\t\tNextHopAddr\t\tInterfaceAddr\t\tCost");

	PRINT_LOG ("");

  //print routing table
  for(std::map<uint32_t, routingTableEntry>::const_iterator it = routingTable.begin(); it != routingTable.end(); it++)
    {
      PRINT_LOG(std::setw(10) << it->first << "\t\t" << std::setw(2) << it->second.destinationAddress << "\t\t" 
                 << std::setw(13) << it->second.nextHopNodeNum << "\t\t" << std::setw(5) << it->second.nextHopAddress << "\t\t" 
                 << std::setw(7) << it->second.interfaceAddress.GetLocal() << "\t\t" << std::setw(4) << it->second.cost << "\t\t");
    }
}

void
LSRoutingProtocol::RecvLSMessage (Ptr<Socket> socket)
{
  Address sourceAddr;
  Ptr<Packet> packet = socket->RecvFrom (sourceAddr);
  InetSocketAddress inetSocketAddr = InetSocketAddress::ConvertFrom (sourceAddr);
  Ipv4Address sourceAddress = inetSocketAddr.GetIpv4 ();
  LSMessage lsMessage;
  packet->RemoveHeader (lsMessage);

  switch (lsMessage.GetMessageType ())
    {
      case LSMessage::PING_REQ:
        ProcessPingReq (lsMessage);
        break;
      case LSMessage::PING_RSP:
        ProcessPingRsp (lsMessage);
        break;

      //new cases for neighbor req and neighbor rsp
      case LSMessage::NEIGHBOR_REQ:
        ProcessNeighborReq (lsMessage, socket);
        break;
      case LSMessage::NEIGHBOR_RSP:
        ProcessNeighborRsp (lsMessage, sourceAddress, socket);
        break;
		    
      //new case for ls packet
      case LSMessage::LSP:
        ProcessLSPacket (lsMessage);
        break;

      default:
        ERROR_LOG ("Unknown Message Type!");
        break;
    }
}

void
LSRoutingProtocol::ProcessPingReq (LSMessage lsMessage)
{
  // Check destination address
  if (IsOwnAddress (lsMessage.GetPingReq().destinationAddress))
    {
      // Use reverse lookup for ease of debug
      std::string fromNode = ReverseLookup (lsMessage.GetOriginatorAddress ());
      TRAFFIC_LOG ("Received PING_REQ, From Node: " << fromNode << ", Message: " << lsMessage.GetPingReq().pingMessage);
      // Send Ping Response
      LSMessage lsResp = LSMessage (LSMessage::PING_RSP, lsMessage.GetSequenceNumber(), m_maxTTL, m_mainAddress);
      lsResp.SetPingRsp (lsMessage.GetOriginatorAddress(), lsMessage.GetPingReq().pingMessage);
      Ptr<Packet> packet = Create<Packet> ();
      packet->AddHeader (lsResp);
      BroadcastPacket (packet);
    }
}

void
LSRoutingProtocol::ProcessPingRsp (LSMessage lsMessage)
{
  // Check destination address
  if (IsOwnAddress (lsMessage.GetPingRsp().destinationAddress))
    {
      // Remove from pingTracker
      std::map<uint32_t, Ptr<PingRequest> >::iterator iter;
      iter = m_pingTracker.find (lsMessage.GetSequenceNumber ());
      if (iter != m_pingTracker.end ())
        {
          std::string fromNode = ReverseLookup (lsMessage.GetOriginatorAddress ());
          TRAFFIC_LOG ("Received PING_RSP, From Node: " << fromNode << ", Message: " << lsMessage.GetPingRsp().pingMessage);
          m_pingTracker.erase (iter);
        }
      else
        {
          DEBUG_LOG ("Received invalid PING_RSP!");
        }
    }
}

bool
LSRoutingProtocol::IsOwnAddress (Ipv4Address originatorAddress)
{
  // Check all interfaces
  for (std::map<Ptr<Socket> , Ipv4InterfaceAddress>::const_iterator i = m_socketAddresses.begin (); i != m_socketAddresses.end (); i++)
    {
      Ipv4InterfaceAddress interfaceAddr = i->second;
      if (originatorAddress == interfaceAddr.GetLocal ())
        {
          return true;
        }
    }
  return false;
}

void
LSRoutingProtocol::AuditPings ()
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
        }
      else
        {
          ++iter;
        }
    }
  // Reschedule timer
  m_auditPingsTimer.Schedule (m_pingTimeout); 
}

uint32_t
LSRoutingProtocol::GetNextSequenceNumber ()
{
  return m_currentSequenceNumber++;
}

void 
LSRoutingProtocol::NotifyInterfaceUp (uint32_t i)
{
  m_staticRouting->NotifyInterfaceUp (i);
}
void 
LSRoutingProtocol::NotifyInterfaceDown (uint32_t i)
{
  m_staticRouting->NotifyInterfaceDown (i);
}
void 
LSRoutingProtocol::NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
  m_staticRouting->NotifyAddAddress (interface, address);
}
void 
LSRoutingProtocol::NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
  m_staticRouting->NotifyRemoveAddress (interface, address);
}

void
LSRoutingProtocol::SetIpv4 (Ptr<Ipv4> ipv4)
{
  m_ipv4 = ipv4;
  m_staticRouting->SetIpv4 (m_ipv4);
}

//Periodically send "hello" messages to directly connected nodes at fixed intervals
void
LSRoutingProtocol::SendHello ()
{
  std::string message = "HELLO";
  uint32_t sequenceNum = GetNextSequenceNumber ();
  uint8_t oneHop = 1;
  TRAFFIC_LOG (std::endl << "Sending NEIGHBOR_REQ to all neighbors. MSG: " << message << ", Seq #: " << sequenceNum << ", TTL: " << (unsigned)oneHop << ", Sender Addr: " << m_mainAddress);

  //create and send hello message
  LSMessage greeting = LSMessage (LSMessage::NEIGHBOR_REQ, sequenceNum, oneHop, m_mainAddress);
  greeting.SetNeighborReq(message);
  Ptr<Packet> packet = Create<Packet> ();
  packet->AddHeader (greeting);
  BroadcastPacket(packet);

  // Reschedule so that Hello messages are sent repeatedly at fixed intervals
  Simulator::Schedule (m_helloPeriod, &LSRoutingProtocol::SendHello, this);
}

//Process "HELLO" neighbor request message from directly connected node, then send neighbor response message to node that sent the request message
void
LSRoutingProtocol::ProcessNeighborReq (LSMessage lsMessage, Ptr<Socket> socket)
{
  // Use reverse lookup for ease of debug
  std::string fromNode = ReverseLookup (lsMessage.GetOriginatorAddress ());
  TRAFFIC_LOG (std::endl << "Received NEIGHBOR_REQ from Node: " << fromNode << ", MSG: " << lsMessage.GetNeighborReq().neighborMessage);
  
  std::string message = "HELLO REPLY"; 
  uint8_t oneHop = 1;
  LSMessage lsResp = LSMessage (LSMessage::NEIGHBOR_RSP, lsMessage.GetSequenceNumber(), oneHop, m_mainAddress);
  lsResp.SetNeighborRsp (lsMessage.GetOriginatorAddress(), message);
  Ptr<Packet> packet = Create<Packet> ();
  packet->AddHeader (lsResp);
  Ipv4Address srcAddress = m_socketAddresses[socket].GetLocal().GetSubnetDirectedBroadcast(m_socketAddresses[socket].GetMask());
  
  TRAFFIC_LOG (std::endl << "Sending NEIGHBOR_RSP to Node: " << fromNode << ", MSG: " << message << ", Seq #: " << lsMessage.GetSequenceNumber() << ", TTL: " << unsigned(oneHop) << ", Sender Addr: " << m_mainAddress);
  
  //send neighbor rsp packet to original sender of neighbor req
  socket->SendTo(packet, 0, InetSocketAddress(srcAddress, m_lsPort));
}

//Process "HELLO REPLY" neighbor response message from directly connected node and update neighbor table
void
LSRoutingProtocol::ProcessNeighborRsp (LSMessage lsMessage, Ipv4Address sourceAddress, Ptr<Socket> socket)
{
  Ipv4InterfaceAddress interfaceIP = m_socketAddresses[socket];

  //Check destination address
  if (IsOwnAddress (lsMessage.GetNeighborRsp().destinationAddress))
    {
      //get number of node that sent neighbor response message
      uint32_t nodeNum = atoi(ReverseLookup(lsMessage.GetOriginatorAddress()).c_str());

      TRAFFIC_LOG (std::endl << "Received NEIGHBOR_RSP From Node: " << nodeNum << ", MSG: " << lsMessage.GetNeighborRsp().neighborMessage);
      
      //add/update entry to neighbor table
      addOrUpdateNeighbor(nodeNum, lsMessage.GetOriginatorAddress(), sourceAddress, interfaceIP, Simulator::Now());
    }
}

//updates entry if neighbor already present, add new entry if neighbor is new entry
void
LSRoutingProtocol::addOrUpdateNeighbor(uint32_t nodeNum, Ipv4Address neighborIP, Ipv4Address sourceInterfaceAddress, Ipv4InterfaceAddress interfaceIP, Time timeLastRefreshed)
{
  //search for node number
  std::map<uint32_t, neighborEntryValue>::iterator search = neighborTable.find(nodeNum);

  bool tableModified = false;

  //if nodeNum is already in the table, update it with more recent time
  if (search != neighborTable.end())
    {

      if( (search->second.neighborIPAddress != neighborIP) ||
          (search->second.neighborIPInterface != sourceInterfaceAddress) ||
          (search->second.interfaceIPAddress != interfaceIP) )
      {
        tableModified = true;
      }

      search->second.neighborIPAddress = neighborIP;
      search->second.neighborIPInterface = sourceInterfaceAddress;
      search->second.interfaceIPAddress = interfaceIP;
      search->second.timeLastRefreshed = timeLastRefreshed;
    }
  //else nodeNum is new to the table, add it as a new entry
  else
    {
      neighborEntryValue tempEntry;
      tempEntry.neighborIPAddress = neighborIP;
	    tempEntry.neighborIPInterface = sourceInterfaceAddress;
      tempEntry.interfaceIPAddress = interfaceIP;
      tempEntry.timeLastRefreshed = timeLastRefreshed;
      neighborTable[nodeNum] = tempEntry;

      tableModified = true;

      //Broadcast neighbor list as LSPacket when new neighbor is found
      SendNeighborList();
    }

  if(tableModified)
    {
	//recompute local routing table if immediate neighbor table changes
      ComputeRoutingTable();
    }
}

//periodically checks neighbor table and deletes outdated entries
void 
LSRoutingProtocol::auditNeighborTable()
{
  bool tableModified = false;

  std::map<uint32_t, neighborEntryValue>::iterator iter;
  for (iter = neighborTable.begin (); iter != neighborTable.end();)
    {
      neighborEntryValue entry = iter->second;
      if (entry.timeLastRefreshed + m_neighborTimeout <= Simulator::Now())
        {
          DEBUG_LOG ("Neighbor entry expired. Neighbor IP: " << entry.neighborIPAddress << ", Interface IP: " << entry.interfaceIPAddress.GetLocal() << ", Time Last Refreshed: " << entry.timeLastRefreshed.GetMilliSeconds() << ", Current Time: " << Simulator::Now().GetMilliSeconds());

          // Remove stale entry
          neighborTable.erase (iter++);
          tableModified = true;
        }
      else
        {
          ++iter;
        }
    }
  
  if(tableModified)
    {
      //recompute local routing table if immediate neighbor table is modified
      ComputeRoutingTable();

      //broadcast neighbor list as LSPacket if neighbor table is modified
      SendNeighborList();
    }

  // Reschedule so that audit occurs periodically
  Simulator::Schedule (m_neighborTimeout, &LSRoutingProtocol::auditNeighborTable, this);
}

//broadcast list of neighbors as LSPacket
void 
LSRoutingProtocol::SendNeighborList ()
{
  uint32_t sequenceNum = GetNextSequenceNumber ();
  uint8_t oneHop = 1;
  TRAFFIC_LOG (std::endl << "Sending home LSP to all neighbors. Seq #: " << sequenceNum << ", Sender Addr: " << m_mainAddress);

  //create and send neighbor list (LSPacket)
  LSMessage myNeighbors = LSMessage (LSMessage::LSP, sequenceNum, oneHop, m_mainAddress);

  std::set<uint32_t> neighbors;
  for(std::map<uint32_t, neighborEntryValue>::const_iterator it = neighborTable.begin(); it != neighborTable.end(); it++)
    {
      neighbors.insert(it->first);
    }

  myNeighbors.SetLSPacket(neighbors);
  Ptr<Packet> packet = Create<Packet> ();
  packet->AddHeader (myNeighbors);
  BroadcastPacket(packet);
}

//ProcessLSPacket takes a LSPacket and checks if its contents are new to the
//relationshipTable (ie new entry or updated entry). If its contents are new, the packet
//is then broadcasted to all neighbors
void
LSRoutingProtocol::ProcessLSPacket (LSMessage lsMessage)
{
  //Check that the packet originally came from a different node
  if (! IsOwnAddress(lsMessage.GetOriginatorAddress()))
  {
    // Use reverse lookup for ease of debug
    std::string fromNode = ReverseLookup (lsMessage.GetOriginatorAddress ());

    TRAFFIC_LOG (std::endl << "Received LSP originally from Node: " << fromNode);

    //get number of node that sent original message
    uint32_t nodeNum = atoi(fromNode.c_str());

    LSMessage::LSPacket lsp = lsMessage.GetLSPacket();

    //search for node number
    std::map<uint32_t, neighborListEntry >::iterator search = relationshipTable.find(nodeNum);

    if((search != relationshipTable.end() && search->second.neighbors != lsp.neighbors && search->second.seqNum < lsMessage.GetSequenceNumber()) ||
      search == relationshipTable.end())
    {
      //relationship table needs an entry update or addition
      relationshipTable[nodeNum].neighbors = lsp.neighbors;
      relationshipTable[nodeNum].seqNum = lsMessage.GetSequenceNumber();

      //recompute routing table
      ComputeRoutingTable();

      //broadcast packet
      Ptr<Packet> packet = Create<Packet> ();
      packet->AddHeader (lsMessage);
      BroadcastPacket(packet);   

      TRAFFIC_LOG (std::endl << "Sending LSP from " << fromNode << " to all neighbors."); 
    }
  }
}

//recompute routing table using immediate neighbor table and relationship table of other nodes
//Applies BFS tactic for O(V+E) time, superior to traditional O(VlgV + E) Dijkstra's time using priority queue
void 
LSRoutingProtocol::ComputeRoutingTable()
{
  //start from blank table
  routingTable.clear();

  std::queue<routingTableEntryShort> myqueue;

  std::set<uint32_t> seen; //nodes already seen

  //add home node to seen to avoid self
  uint32_t myNodeNum = atoi((ReverseLookup (m_mainAddress)).c_str());
  seen.insert(myNodeNum);

  //add immediate neighbors to myqueue and seen
  for(std::map<uint32_t, neighborEntryValue>::const_iterator it = neighborTable.begin(); it != neighborTable.end(); it++)
    {
      uint32_t neighborNodeNum = it->first;
      routingTableEntryShort temp;
      temp.destNodeNum = neighborNodeNum;
      temp.nextHopNodeNum = neighborNodeNum;
      temp.cost = 1;

      myqueue.push(temp);
      seen.insert(neighborNodeNum);
    }

  //Breadth-First-Search
  while(!myqueue.empty())
    {
      //dequeue next node in line
      routingTableEntryShort node = myqueue.front();
      myqueue.pop();

      uint32_t nodeNum = node.destNodeNum;

      //process node to add to routing table
      routingTableEntry tmpEntry;
      tmpEntry.destinationAddress = ResolveNodeIpAddress(node.destNodeNum);
      tmpEntry.nextHopNodeNum = node.nextHopNodeNum;
      tmpEntry.nextHopAddress = ResolveNodeIpAddress(node.nextHopNodeNum);
      tmpEntry.interfaceAddress = neighborTable[node.nextHopNodeNum].interfaceIPAddress;
      tmpEntry.cost = node.cost;
      routingTable[nodeNum] = tmpEntry;

      //search for node number in cumulative neighbor tables
      std::map<uint32_t, neighborListEntry >::iterator search = relationshipTable.find(nodeNum);

      //if nodeNum is in the relationship table
      if (search != relationshipTable.end())
        {
          //add nodeNum's neighbors to myqueue and seen
          for(std::set<uint32_t>::const_iterator it = search->second.neighbors.begin(); it != search->second.neighbors.end(); it++)
            {
              if(seen.find(*it) == seen.end())
                {
                  //seen does not contain *it
                  routingTableEntryShort temp;
                  temp.destNodeNum = *it;
                  temp.nextHopNodeNum = node.nextHopNodeNum;
                  temp.cost = node.cost + 1;

                  myqueue.push(temp);
                  seen.insert(*it);
                }
            }
        }
    }
}