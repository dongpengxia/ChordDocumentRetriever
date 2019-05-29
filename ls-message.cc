#include "ns3/log.h"
#include "ns3/ls-message.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LSMessage");
NS_OBJECT_ENSURE_REGISTERED (LSMessage);

LSMessage::LSMessage () {}

LSMessage::~LSMessage () {}

LSMessage::LSMessage (LSMessage::MessageType messageType, uint32_t sequenceNumber, uint8_t ttl, Ipv4Address originatorAddress)
{
  m_messageType = messageType;
  m_sequenceNumber = sequenceNumber;
  m_ttl = ttl;
  m_originatorAddress = originatorAddress;
}

TypeId 
LSMessage::GetTypeId (void)
{
  static TypeId tid = TypeId ("LSMessage")
    .SetParent<Header> ()
    .AddConstructor<LSMessage> ()
  ;
  return tid;
}

TypeId
LSMessage::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

uint32_t
LSMessage::GetSerializedSize (void) const
{
  // size of messageType, sequence number, originator address, ttl
  uint32_t size = sizeof (uint8_t) + sizeof (uint32_t) + IPV4_ADDRESS_SIZE + sizeof (uint8_t);
  switch (m_messageType)
    {
      case PING_REQ:
        size += m_message.pingReq.GetSerializedSize ();
        break;
      case PING_RSP:
        size += m_message.pingRsp.GetSerializedSize ();
        break;
      //cases for neighbor discovery
      case NEIGHBOR_REQ:
        size += m_message.neighborReq.GetSerializedSize ();
        break;
      case NEIGHBOR_RSP:
        size += m_message.neighborRsp.GetSerializedSize ();
        break;
      //case for LSPacket
      case LSP:
        size += m_message.lsPacket.GetSerializedSize ();
        break;
      default:
        NS_ASSERT (false);
    }
  return size;
}

void
LSMessage::Print (std::ostream &os) const
{
  os << "\n****LSMessage Dump****\n" ;
  os << "messageType: " << m_messageType << "\n";
  os << "sequenceNumber: " << m_sequenceNumber << "\n";
  os << "ttl: " << m_ttl << "\n";
  os << "originatorAddress: " << m_originatorAddress << "\n";
  os << "PAYLOAD:: \n";
  
  switch (m_messageType)
    {
      case PING_REQ:
        m_message.pingReq.Print (os);
        break;
      case PING_RSP:
        m_message.pingRsp.Print (os);
        break;
      //cases for neighbor discovery
      case NEIGHBOR_REQ:
        m_message.neighborReq.Print (os);
        break;
      case NEIGHBOR_RSP:
        m_message.neighborRsp.Print (os);
        break;
      //case for LSPacket
      case LSP:
        m_message.lsPacket.Print (os);
        break;
      default:
        break;  
    }
  os << "\n****END OF MESSAGE****\n";
}

void
LSMessage::Serialize (Buffer::Iterator start) const
{
  Buffer::Iterator i = start;
  i.WriteU8 (m_messageType);
  i.WriteHtonU32 (m_sequenceNumber);
  i.WriteU8 (m_ttl);
  i.WriteHtonU32 (m_originatorAddress.Get ());

  switch (m_messageType)
    {
      case PING_REQ:
        m_message.pingReq.Serialize (i);
        break;
      case PING_RSP:
        m_message.pingRsp.Serialize (i);
        break;
      //cases for neighbor discovery
      case NEIGHBOR_REQ:
        m_message.neighborReq.Serialize (i);
        break;
      case NEIGHBOR_RSP:
        m_message.neighborRsp.Serialize (i);
        break;
      //case for LSPacket
      case LSP:
        m_message.lsPacket.Serialize (i);
        break;
      default:
        NS_ASSERT (false);   
    }
}

uint32_t 
LSMessage::Deserialize (Buffer::Iterator start)
{
  uint32_t size;
  Buffer::Iterator i = start;
  m_messageType = (MessageType) i.ReadU8 ();
  m_sequenceNumber = i.ReadNtohU32 ();
  m_ttl = i.ReadU8 ();
  m_originatorAddress = Ipv4Address (i.ReadNtohU32 ());

  size = sizeof (uint8_t) + sizeof (uint32_t) + sizeof (uint8_t) + IPV4_ADDRESS_SIZE;

  switch (m_messageType)
    {
      case PING_REQ:
        size += m_message.pingReq.Deserialize (i);
        break;
      case PING_RSP:
        size += m_message.pingRsp.Deserialize (i);
        break;
      //cases for neighbor discovery
      case NEIGHBOR_REQ:
        size += m_message.neighborReq.Deserialize (i);
        break;
      case NEIGHBOR_RSP:
        size += m_message.neighborRsp.Deserialize (i);
        break;
      //case for LSPacket
      case LSP:
        size += m_message.lsPacket.Deserialize (i);
        break;
      default:
        NS_ASSERT (false);
    }
  return size;
}

/* PING_REQ */

uint32_t 
LSMessage::PingReq::GetSerializedSize (void) const
{
  uint32_t size;
  size = IPV4_ADDRESS_SIZE + sizeof(uint16_t) + pingMessage.length();
  return size;
}

void
LSMessage::PingReq::Print (std::ostream &os) const
{
  os << "PingReq:: Message: " << pingMessage << "\n";
}

void
LSMessage::PingReq::Serialize (Buffer::Iterator &start) const
{
  start.WriteHtonU32 (destinationAddress.Get ());
  start.WriteU16 (pingMessage.length ());
  start.Write ((uint8_t *) (const_cast<char*> (pingMessage.c_str())), pingMessage.length());
}

uint32_t
LSMessage::PingReq::Deserialize (Buffer::Iterator &start)
{  
  destinationAddress = Ipv4Address (start.ReadNtohU32 ());
  uint16_t length = start.ReadU16 ();
  char* str = (char*) malloc (length);
  start.Read ((uint8_t*)str, length);
  pingMessage = std::string (str, length);
  free (str);
  return PingReq::GetSerializedSize ();
}

void
LSMessage::SetPingReq (Ipv4Address destinationAddress, std::string pingMessage)
{
  if (m_messageType == 0)
    {
      m_messageType = PING_REQ;
    }
  else
    {
      NS_ASSERT (m_messageType == PING_REQ);
    }
  m_message.pingReq.destinationAddress = destinationAddress;
  m_message.pingReq.pingMessage = pingMessage;
}

LSMessage::PingReq
LSMessage::GetPingReq ()
{
  return m_message.pingReq;
}

/* PING_RSP */

uint32_t 
LSMessage::PingRsp::GetSerializedSize (void) const
{
  uint32_t size;
  size = IPV4_ADDRESS_SIZE + sizeof(uint16_t) + pingMessage.length();
  return size;
}

void
LSMessage::PingRsp::Print (std::ostream &os) const
{
  os << "PingReq:: Message: " << pingMessage << "\n";
}

void
LSMessage::PingRsp::Serialize (Buffer::Iterator &start) const
{
  start.WriteHtonU32 (destinationAddress.Get ());
  start.WriteU16 (pingMessage.length ());
  start.Write ((uint8_t *) (const_cast<char*> (pingMessage.c_str())), pingMessage.length());
}

uint32_t
LSMessage::PingRsp::Deserialize (Buffer::Iterator &start)
{  
  destinationAddress = Ipv4Address (start.ReadNtohU32 ());
  uint16_t length = start.ReadU16 ();
  char* str = (char*) malloc (length);
  start.Read ((uint8_t*)str, length);
  pingMessage = std::string (str, length);
  free (str);
  return PingRsp::GetSerializedSize ();
}

void
LSMessage::SetPingRsp (Ipv4Address destinationAddress, std::string pingMessage)
{
  if (m_messageType == 0)
    {
      m_messageType = PING_RSP;
    }
  else
    {
      NS_ASSERT (m_messageType == PING_RSP);
    }
  m_message.pingRsp.destinationAddress = destinationAddress;
  m_message.pingRsp.pingMessage = pingMessage;
}

LSMessage::PingRsp
LSMessage::GetPingRsp ()
{
  return m_message.pingRsp;
}

//NeighborReq

uint32_t 
LSMessage::NeighborReq::GetSerializedSize (void) const
{
  uint32_t size;
  size = sizeof(uint16_t) + neighborMessage.length();
  return size;
}

void
LSMessage::NeighborReq::Print (std::ostream &os) const
{
  os << "NeighborReq:: Message: " << neighborMessage << "\n";
}

void
LSMessage::NeighborReq::Serialize (Buffer::Iterator &start) const
{
  start.WriteU16 (neighborMessage.length ());
  start.Write ((uint8_t *) (const_cast<char*> (neighborMessage.c_str())), neighborMessage.length());
}

uint32_t
LSMessage::NeighborReq::Deserialize (Buffer::Iterator &start)
{  
  uint16_t length = start.ReadU16 ();
  char* str = (char*) malloc (length);
  start.Read ((uint8_t*)str, length);
  neighborMessage = std::string (str, length);
  free (str);
  return NeighborReq::GetSerializedSize ();
}

void
LSMessage::SetNeighborReq (std::string message)
{
  if (m_messageType == 0)
    {
      m_messageType = NEIGHBOR_REQ;
    }
  else
    {
      NS_ASSERT (m_messageType == NEIGHBOR_REQ);
    }
  m_message.neighborReq.neighborMessage = message;
}

LSMessage::NeighborReq
LSMessage::GetNeighborReq ()
{
  return m_message.neighborReq;
}

//NeighborRsp

uint32_t 
LSMessage::NeighborRsp::GetSerializedSize (void) const
{
  uint32_t size;
  size = IPV4_ADDRESS_SIZE + sizeof(uint16_t) + neighborMessage.length();
  return size;
}

void
LSMessage::NeighborRsp::Print (std::ostream &os) const
{
  os << "NeighborRsp:: Message: " << neighborMessage << "\n";
}

void
LSMessage::NeighborRsp::Serialize (Buffer::Iterator &start) const
{
  start.WriteHtonU32 (destinationAddress.Get ());
  start.WriteU16 (neighborMessage.length ());
  start.Write ((uint8_t *) (const_cast<char*> (neighborMessage.c_str())), neighborMessage.length());
}

uint32_t
LSMessage::NeighborRsp::Deserialize (Buffer::Iterator &start)
{  
  destinationAddress = Ipv4Address (start.ReadNtohU32 ());
  uint16_t length = start.ReadU16 ();
  char* str = (char*) malloc (length);
  start.Read ((uint8_t*)str, length);
  neighborMessage = std::string (str, length);
  free (str);
  return NeighborRsp::GetSerializedSize ();
}

void
LSMessage::SetNeighborRsp (Ipv4Address destinationAddress, std::string message)
{
  if (m_messageType == 0)
    {
      m_messageType = NEIGHBOR_RSP;
    }
  else
    {
      NS_ASSERT (m_messageType == NEIGHBOR_RSP);
    }
  m_message.neighborRsp.destinationAddress = destinationAddress;
  m_message.neighborRsp.neighborMessage = message;
}

LSMessage::NeighborRsp
LSMessage::GetNeighborRsp ()
{
  return m_message.neighborRsp;
}

//LSPacket

uint32_t 
LSMessage::LSPacket::GetSerializedSize (void) const
{
  return (sizeof(uint16_t) + neighbors.size() * sizeof(uint32_t));
}

void
LSMessage::LSPacket::Print (std::ostream &os) const
{
  os << "LSPacket:: neighbors: ";
  std::set<uint32_t>::iterator it;
  for(it = neighbors.begin(); it != neighbors.end(); it++)
    {
      os << *it << " ";
    }
   os << "\n";
}

void
LSMessage::LSPacket::Serialize (Buffer::Iterator &start) const
{
  start.WriteU16 (neighbors.size());
  std::set<uint32_t>::iterator it;
  for(it = neighbors.begin(); it != neighbors.end(); it++)
    {
      start.WriteHtonU32(*it);
    }
}

uint32_t
LSMessage::LSPacket::Deserialize (Buffer::Iterator &start)
{  
  uint16_t length = start.ReadU16 ();
  uint32_t nextNeighbor;
  for(uint16_t i = 0; i < length; i++)
  {
    nextNeighbor = start.ReadNtohU32 ();
    neighbors.insert(nextNeighbor);
  }
  return LSPacket::GetSerializedSize ();
}

void
LSMessage::SetLSPacket (std::set<uint32_t> neighbors)
{
  if (m_messageType == 0)
    {
      m_messageType = LSP;
    }
  else
    {
      NS_ASSERT (m_messageType == LSP);
    }
  m_message.lsPacket.neighbors = neighbors;
}

LSMessage::LSPacket
LSMessage::GetLSPacket ()
{
  return m_message.lsPacket;
}

void
LSMessage::SetMessageType (MessageType messageType)
{
  m_messageType = messageType;
}

LSMessage::MessageType
LSMessage::GetMessageType () const
{
  return m_messageType;
}

void
LSMessage::SetSequenceNumber (uint32_t sequenceNumber)
{
  m_sequenceNumber = sequenceNumber;
}

uint32_t 
LSMessage::GetSequenceNumber (void) const
{
  return m_sequenceNumber;
}

void
LSMessage::SetTTL (uint8_t ttl)
{
  m_ttl = ttl;
}

uint8_t 
LSMessage::GetTTL (void) const
{
  return m_ttl;
}

void
LSMessage::SetOriginatorAddress (Ipv4Address originatorAddress)
{
  m_originatorAddress = originatorAddress;
}

Ipv4Address
LSMessage::GetOriginatorAddress (void) const
{
  return m_originatorAddress;
}