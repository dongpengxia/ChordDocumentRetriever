#include "ns3/gu-chord-message.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("GUChordMessage");
NS_OBJECT_ENSURE_REGISTERED (GUChordMessage);

GUChordMessage::GUChordMessage ()
{
}

GUChordMessage::~GUChordMessage ()
{
}

GUChordMessage::GUChordMessage (GUChordMessage::MessageType messageType, uint32_t transactionId)
{
  m_messageType = messageType;
  m_transactionId = transactionId;
}

TypeId 
GUChordMessage::GetTypeId (void)
{
  static TypeId tid = TypeId ("GUChordMessage")
    .SetParent<Header> ()
    .AddConstructor<GUChordMessage> ()
  ;
  return tid;
}

TypeId
GUChordMessage::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}


uint32_t
GUChordMessage::GetSerializedSize (void) const
{
  // size of messageType, transaction id
  uint32_t size = sizeof (uint8_t) + sizeof (uint32_t);
  switch (m_messageType)
    {
      case PING_REQ:
        size += m_message.pingReq.GetSerializedSize ();
        break;
      case PING_RSP:
        size += m_message.pingRsp.GetSerializedSize ();
        break;
      case SUCCESSOR_REQ:
        size += m_message.successorReq.GetSerializedSize();
        break;
      case SUCCESSOR_RSP:
        size += m_message.successorRsp.GetSerializedSize();
        break;
      case NOTIFY_SUCCESSOR:
        break;
      case NOTIFY_PREDECESSOR:
        break;
      case GET_PREDECESSOR_OF_SUCCESSOR_REQ:
        break;
      case GET_PREDECESSOR_OF_SUCCESSOR_RSP:
        size += m_message.predecessorOfSuccessorRsp.GetSerializedSize();
        break;
      case RINGSTATE_REQ:
        size += m_message.ringstateReq.GetSerializedSize();
        break;
      case LOOKUP_REQ:
        size += m_message.lookupReq.GetSerializedSize();
        break;
      case LOOKUP_RSP:
        size += m_message.lookupRsp.GetSerializedSize();
        break;
      case NODELEAVE:
        size += m_message.nodeLeave.GetSerializedSize();
        break;
      default:
        NS_ASSERT (false);
    }
  return size;
}

void
GUChordMessage::Print (std::ostream &os) const
{
  os << "\n****GUChordMessage Dump****\n" ;
  os << "messageType: " << m_messageType << "\n";
  os << "transactionId: " << m_transactionId << "\n";
  os << "PAYLOAD:: \n";
  
  switch (m_messageType)
    {
      case PING_REQ:
        m_message.pingReq.Print (os);
        break;
      case PING_RSP:
        m_message.pingRsp.Print (os);
        break;
      case SUCCESSOR_REQ:
        m_message.successorReq.Print(os);
        break;
      case SUCCESSOR_RSP:
        m_message.successorRsp.Print(os);
        break;
      case NOTIFY_SUCCESSOR:
        break;
      case NOTIFY_PREDECESSOR:
        break;
      case GET_PREDECESSOR_OF_SUCCESSOR_REQ:
        break;
      case GET_PREDECESSOR_OF_SUCCESSOR_RSP:
        m_message.predecessorOfSuccessorRsp.Print(os);
        break;
      case RINGSTATE_REQ:
        m_message.ringstateReq.Print(os);
        break;
      case LOOKUP_REQ:
        m_message.lookupReq.Print(os);
        break;
      case LOOKUP_RSP:
        m_message.lookupRsp.Print(os);
        break;
      case NODELEAVE:
        m_message.nodeLeave.Print(os);
        break;
      default:
        break;  
    }
  os << "\n****END OF MESSAGE****\n";
}

void
GUChordMessage::Serialize (Buffer::Iterator start) const
{
  Buffer::Iterator i = start;
  i.WriteU8 (m_messageType);
  i.WriteHtonU32 (m_transactionId);

  switch (m_messageType)
    {
      case PING_REQ:
        m_message.pingReq.Serialize (i);
        break;
      case PING_RSP:
        m_message.pingRsp.Serialize (i);
        break;
      case SUCCESSOR_REQ:
        m_message.successorReq.Serialize(i);
        break;
      case SUCCESSOR_RSP:
        m_message.successorRsp.Serialize(i);
        break;
      case NOTIFY_SUCCESSOR:
        break;
      case NOTIFY_PREDECESSOR:
        break;
      case GET_PREDECESSOR_OF_SUCCESSOR_REQ:
        break;
      case GET_PREDECESSOR_OF_SUCCESSOR_RSP:
        m_message.predecessorOfSuccessorRsp.Serialize(i);
        break;
      case RINGSTATE_REQ:
        m_message.ringstateReq.Serialize(i);
        break;
      case LOOKUP_REQ:
        m_message.lookupReq.Serialize(i);
        break;
      case LOOKUP_RSP:
        m_message.lookupRsp.Serialize(i);
        break;
      case NODELEAVE:
        m_message.nodeLeave.Serialize(i);
        break;
      default:
        NS_ASSERT (false);   
    }
}

uint32_t 
GUChordMessage::Deserialize (Buffer::Iterator start)
{
  uint32_t size;
  Buffer::Iterator i = start;
  m_messageType = (MessageType) i.ReadU8 ();
  m_transactionId = i.ReadNtohU32 ();

  size = sizeof (uint8_t) + sizeof (uint32_t);

  switch (m_messageType)
    {
      case PING_REQ:
        size += m_message.pingReq.Deserialize (i);
        break;
      case PING_RSP:
        size += m_message.pingRsp.Deserialize (i);
        break;
      case SUCCESSOR_REQ:
        size += m_message.successorReq.Deserialize(i);
        break;
      case SUCCESSOR_RSP:
        size += m_message.successorRsp.Deserialize(i);
        break;
      case NOTIFY_SUCCESSOR:
        break;
      case NOTIFY_PREDECESSOR:
        break;
      case GET_PREDECESSOR_OF_SUCCESSOR_REQ:
        break;
      case GET_PREDECESSOR_OF_SUCCESSOR_RSP:
        size += m_message.predecessorOfSuccessorRsp.Deserialize(i);
        break;
      case RINGSTATE_REQ:
        size += m_message.ringstateReq.Deserialize(i);
        break;
      case LOOKUP_REQ:
        size += m_message.lookupReq.Deserialize(i);
        break;
      case LOOKUP_RSP:
        size += m_message.lookupRsp.Deserialize(i);
        break;
      case NODELEAVE:
        size += m_message.nodeLeave.Deserialize(i);
        break;
      default:
        NS_ASSERT (false);
    }
  return size;
}

//SUCCESSOR_REQ
void
GUChordMessage::SuccessorReq::Print (std::ostream &os) const 
{
  os << "SuccessorReq:: Original Requester is: " << originalNodeNum << " Key: " << chordKey << std::endl;
}

uint32_t
GUChordMessage::SuccessorReq::GetSerializedSize(void) const 
{
  uint32_t size;
  size = sizeof(uint32_t) + sizeof(uint32_t);
  return size;
}

void
GUChordMessage::SuccessorReq::Serialize(Buffer::Iterator &start) const 
{
  start.WriteU32(originalNodeNum);
  start.WriteU32(chordKey);
}

uint32_t
GUChordMessage::SuccessorReq::Deserialize(Buffer::Iterator &start) 
{
    originalNodeNum = start.ReadU32();
    chordKey = start.ReadU32();
    return SuccessorReq::GetSerializedSize();
}

void
GUChordMessage::SetSuccessorReq(uint32_t originalNodeNum, uint32_t chordKey) 
{
  if (m_messageType == 0) 
    {
      m_messageType = SUCCESSOR_REQ;
    } 
  else 
    {
      NS_ASSERT (m_messageType == SUCCESSOR_REQ);
    }
  m_message.successorReq.originalNodeNum = originalNodeNum;
  m_message.successorReq.chordKey = chordKey;
}

GUChordMessage::SuccessorReq
GUChordMessage::GetSuccessorReq() 
{
  return m_message.successorReq;
}

//SUCCESSOR_RSP
void
GUChordMessage::SuccessorRsp::Print (std::ostream &os) const 
{
  os << "SuccessorRsp:: Successor is: " << successorNodeNum << " Key: " << successorChordKey << std::endl;
}

uint32_t
GUChordMessage::SuccessorRsp::GetSerializedSize(void) const 
{
  uint32_t size;
  size = sizeof(uint32_t) + sizeof(uint32_t);
  return size;
}

void
GUChordMessage::SuccessorRsp::Serialize(Buffer::Iterator &start) const 
{
  start.WriteU32(successorNodeNum);
  start.WriteU32(successorChordKey);
}

uint32_t
GUChordMessage::SuccessorRsp::Deserialize(Buffer::Iterator &start) 
{
    successorNodeNum = start.ReadU32();
    successorChordKey = start.ReadU32();
    return SuccessorRsp::GetSerializedSize();
}

void
GUChordMessage::SetSuccessorRsp(uint32_t successorNodeNum, uint32_t successorChordKey) 
{
  if (m_messageType == 0) 
    {
      m_messageType = SUCCESSOR_RSP;
    } 
  else 
    {
      NS_ASSERT (m_messageType == SUCCESSOR_RSP);
    }
  m_message.successorRsp.successorNodeNum = successorNodeNum;
  m_message.successorRsp.successorChordKey = successorChordKey;
}

GUChordMessage::SuccessorRsp
GUChordMessage::GetSuccessorRsp() 
{
  return m_message.successorRsp;
}

//GET_PREDECESSOR_OF_SUCCESSOR_RSP
void
GUChordMessage::PredecessorOfSuccessorRsp::Print (std::ostream &os) const 
{
  os << "GetPredecessorOfSuccessorRsp:: Predecessor is: " << predecessorNodeNum << " Key: " << predecessorChordKey << std::endl;
}

uint32_t
GUChordMessage::PredecessorOfSuccessorRsp::GetSerializedSize(void) const 
{
  uint32_t size;
  size = sizeof(uint32_t) + sizeof(uint32_t);
  return size;
}

void
GUChordMessage::PredecessorOfSuccessorRsp::Serialize(Buffer::Iterator &start) const 
{
  start.WriteU32(predecessorNodeNum);
  start.WriteU32(predecessorChordKey);
}

uint32_t
GUChordMessage::PredecessorOfSuccessorRsp::Deserialize(Buffer::Iterator &start) 
{
  predecessorNodeNum = start.ReadU32();
  predecessorChordKey = start.ReadU32();
  return PredecessorOfSuccessorRsp::GetSerializedSize();
}

void
GUChordMessage::SetPredecessorOfSuccessorRsp(uint32_t predecessorNodeNum, uint32_t predecessorChordKey) 
{
  if (m_messageType == 0) 
    {
      m_messageType = GET_PREDECESSOR_OF_SUCCESSOR_RSP;
    } 
  else 
    {
      NS_ASSERT (m_messageType == GET_PREDECESSOR_OF_SUCCESSOR_RSP);
    }
  m_message.predecessorOfSuccessorRsp.predecessorNodeNum = predecessorNodeNum;
  m_message.predecessorOfSuccessorRsp.predecessorChordKey = predecessorChordKey;
}

GUChordMessage::PredecessorOfSuccessorRsp
GUChordMessage::GetPredecessorOfSuccessorRsp() 
{
  return m_message.predecessorOfSuccessorRsp;
}

//RINGSTATE_REQ
void
GUChordMessage::RingstateReq::Print (std::ostream &os) const 
{
  os << "ringstateReq:: Original Requester is: " << originalNodeNum << std::endl;
}

uint32_t
GUChordMessage::RingstateReq::GetSerializedSize(void) const 
{
  uint32_t size;
  size = sizeof(uint32_t);
  return size;
}

void
GUChordMessage::RingstateReq::Serialize(Buffer::Iterator &start) const 
{
  start.WriteU32(originalNodeNum);
}

uint32_t
GUChordMessage::RingstateReq::Deserialize(Buffer::Iterator &start) 
{
  originalNodeNum = start.ReadU32();
  return RingstateReq::GetSerializedSize();
}

void
GUChordMessage::SetRingstateReq(uint32_t originalNodeNum) 
{
  if (m_messageType == 0) 
    {
      m_messageType = RINGSTATE_REQ;
    } 
  else
    {
      NS_ASSERT (m_messageType == RINGSTATE_REQ);
    }
  m_message.ringstateReq.originalNodeNum = originalNodeNum;
}

GUChordMessage::RingstateReq
GUChordMessage::GetRingstateReq() 
{
  return m_message.ringstateReq;
}

//LOOKUP_REQ
void
GUChordMessage::LookupReq::Print (std::ostream &os) const 
{
  os << "LookupReq:: Original Requester is: " << originalNodeNum << " Lookup Key: " << lookupKey;
  if(fromChord == 1)
  {
    os << " Initiated from Chord layer";
  }
  else if(fromChord == 0)
  {
    os << " Initiated outside Chord layer";
  }
  os << std::endl;
}

uint32_t
GUChordMessage::LookupReq::GetSerializedSize(void) const 
{
  uint32_t size;
  size = sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint8_t);
  return size;
}

void
GUChordMessage::LookupReq::Serialize(Buffer::Iterator &start) const 
{
  start.WriteU32(originalNodeNum);
  start.WriteU32(lookupKey);
  start.WriteU8(fromChord);
}

uint32_t
GUChordMessage::LookupReq::Deserialize(Buffer::Iterator &start) 
{
    originalNodeNum = start.ReadU32();
    lookupKey = start.ReadU32();
    fromChord = start.ReadU8();
    return LookupReq::GetSerializedSize();
}

void
GUChordMessage::SetLookupReq(uint32_t originalNodeNum, uint32_t lookupKey, uint8_t fromChord) 
{
  if (m_messageType == 0) 
    {
      m_messageType = LOOKUP_REQ;
    } 
  else 
    {
      NS_ASSERT (m_messageType == LOOKUP_REQ);
    }
  m_message.lookupReq.originalNodeNum = originalNodeNum;
  m_message.lookupReq.lookupKey = lookupKey;
  m_message.lookupReq.fromChord = fromChord;
}

GUChordMessage::LookupReq
GUChordMessage::GetLookupReq() 
{
  return m_message.lookupReq;
}

//LOOKUP_RSP
void
GUChordMessage::LookupRsp::Print (std::ostream &os) const 
{
  os << "LookupRsp:: Key's successor is: " << successorNodeNum << " Lookup Key: " << lookupKey << std::endl;
  if(fromChord == 1)
  {
    os << " Initiated from Chord layer";
  }
  else if(fromChord == 0)
  {
    os << " Initiated outside Chord layer";
  }
  os << std::endl;
}

uint32_t
GUChordMessage::LookupRsp::GetSerializedSize(void) const 
{
  uint32_t size;
  size = sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint8_t);
  return size;
}

void
GUChordMessage::LookupRsp::Serialize(Buffer::Iterator &start) const 
{
  start.WriteU32(successorNodeNum);
  start.WriteU32(lookupKey);
  start.WriteU8(fromChord);
}

uint32_t
GUChordMessage::LookupRsp::Deserialize(Buffer::Iterator &start) 
{
    successorNodeNum = start.ReadU32();
    lookupKey = start.ReadU32();
    fromChord = start.ReadU8();
    return LookupRsp::GetSerializedSize();
}

void
GUChordMessage::SetLookupRsp(uint32_t successorNodeNum, uint32_t lookupKey, uint8_t fromChord) 
{
  if (m_messageType == 0) 
    {
      m_messageType = LOOKUP_RSP;
    } 
  else 
    {
      NS_ASSERT (m_messageType == LOOKUP_RSP);
    }
  m_message.lookupRsp.successorNodeNum = successorNodeNum;
  m_message.lookupRsp.lookupKey = lookupKey;
  m_message.lookupRsp.fromChord = fromChord;
}

GUChordMessage::LookupRsp
GUChordMessage::GetLookupRsp() 
{
  return m_message.lookupRsp;
}

//NODE LEAVE
void
GUChordMessage::NodeLeave::Print(std::ostream &os) const {
  if (isSuccessor)
    os << "New successor- IP: ";
  else 
    os << "New predecessor- IP: ";
  os << ipAddr << " Hashed value:" << hashedNode <<"\n";
  
}

uint32_t
GUChordMessage::NodeLeave::GetSerializedSize(void) const {
  uint32_t size = 0;
  size += sizeof(uint8_t); //issuccessor
  size += sizeof(uint32_t); //hashednode
  size += sizeof(uint32_t); //ipaddr
  return size;
}

void
GUChordMessage::NodeLeave::Serialize(Buffer::Iterator &start) const {
  start.WriteU8(isSuccessor);
  start.WriteU32(hashedNode);
  start.WriteU32(ipAddr);
}

uint32_t
GUChordMessage::NodeLeave::Deserialize (Buffer::Iterator &start) {
  isSuccessor = start.ReadU8();
  hashedNode = start.ReadU32();
  ipAddr = start.ReadU32();
  return NodeLeave::GetSerializedSize();
}

GUChordMessage::NodeLeave
GUChordMessage::GetNodeLeave() {
  return m_message.nodeLeave;
}

void
GUChordMessage::SetNodeLeave(uint8_t isSuccessor, uint32_t hashedNode, uint32_t ipAddr) {
  if (m_messageType == 0) 
    {
      m_messageType = NODELEAVE;
    } 
  else 
    {
      NS_ASSERT (m_messageType == NODELEAVE);
    }
  m_message.nodeLeave.isSuccessor = isSuccessor;
  m_message.nodeLeave.hashedNode = hashedNode;
  m_message.nodeLeave.ipAddr = ipAddr;
}

//PING_REQ
uint32_t 
GUChordMessage::PingReq::GetSerializedSize (void) const
{
  uint32_t size;
  size = sizeof(uint16_t) + pingMessage.length();
  return size;
}

void
GUChordMessage::PingReq::Print (std::ostream &os) const
{
  os << "PingReq:: Message: " << pingMessage << "\n";
}

void
GUChordMessage::PingReq::Serialize (Buffer::Iterator &start) const
{
  start.WriteU16 (pingMessage.length ());
  start.Write ((uint8_t *) (const_cast<char*> (pingMessage.c_str())), pingMessage.length());
}

uint32_t
GUChordMessage::PingReq::Deserialize (Buffer::Iterator &start)
{  
  uint16_t length = start.ReadU16 ();
  char* str = (char*) malloc (length);
  start.Read ((uint8_t*)str, length);
  pingMessage = std::string (str, length);
  free (str);
  return PingReq::GetSerializedSize ();
}

void
GUChordMessage::SetPingReq (std::string pingMessage)
{
  if (m_messageType == 0)
    {
      m_messageType = PING_REQ;
    }
  else
    {
      NS_ASSERT (m_messageType == PING_REQ);
    }
  m_message.pingReq.pingMessage = pingMessage;
}

GUChordMessage::PingReq
GUChordMessage::GetPingReq ()
{
  return m_message.pingReq;
}

//PING_RSP
uint32_t 
GUChordMessage::PingRsp::GetSerializedSize (void) const
{
  uint32_t size;
  size = sizeof(uint16_t) + pingMessage.length();
  return size;
}

void
GUChordMessage::PingRsp::Print (std::ostream &os) const
{
  os << "PingReq:: Message: " << pingMessage << "\n";
}

void
GUChordMessage::PingRsp::Serialize (Buffer::Iterator &start) const
{
  start.WriteU16 (pingMessage.length ());
  start.Write ((uint8_t *) (const_cast<char*> (pingMessage.c_str())), pingMessage.length());
}

uint32_t
GUChordMessage::PingRsp::Deserialize (Buffer::Iterator &start)
{  
  uint16_t length = start.ReadU16 ();
  char* str = (char*) malloc (length);
  start.Read ((uint8_t*)str, length);
  pingMessage = std::string (str, length);
  free (str);
  return PingRsp::GetSerializedSize ();
}

void
GUChordMessage::SetPingRsp (std::string pingMessage)
{
  if (m_messageType == 0)
    {
      m_messageType = PING_RSP;
    }
  else
    {
      NS_ASSERT (m_messageType == PING_RSP);
    }
  m_message.pingRsp.pingMessage = pingMessage;
}

GUChordMessage::PingRsp
GUChordMessage::GetPingRsp ()
{
  return m_message.pingRsp;
}

//Generics

void
GUChordMessage::SetMessageType (MessageType messageType)
{
  m_messageType = messageType;
}

GUChordMessage::MessageType
GUChordMessage::GetMessageType () const
{
  return m_messageType;
}

void
GUChordMessage::SetTransactionId (uint32_t transactionId)
{
  m_transactionId = transactionId;
}

uint32_t 
GUChordMessage::GetTransactionId (void) const
{
  return m_transactionId;
}