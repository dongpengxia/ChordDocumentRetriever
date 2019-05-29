#include "ns3/gu-search-message.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("GUSearchMessage");
NS_OBJECT_ENSURE_REGISTERED (GUSearchMessage);

GUSearchMessage::GUSearchMessage ()
{
}

GUSearchMessage::~GUSearchMessage ()
{
}

GUSearchMessage::GUSearchMessage (GUSearchMessage::MessageType messageType, uint32_t transactionId)
{
  m_messageType = messageType;
  m_transactionId = transactionId;
}

TypeId 
GUSearchMessage::GetTypeId (void)
{
  static TypeId tid = TypeId ("GUSearchMessage")
    .SetParent<Header> ()
    .AddConstructor<GUSearchMessage> ()
  ;
  return tid;
}

TypeId
GUSearchMessage::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}


uint32_t
GUSearchMessage::GetSerializedSize (void) const
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
      case PUBLISH_REQ:
        size += m_message.publishReq.GetSerializedSize();
        break;
      case SEARCH_REQ:
        size += m_message.searchReq.GetSerializedSize();
        break;
      case SEARCH_RSP:
        size += m_message.searchRsp.GetSerializedSize();
        break;
      default:
        NS_ASSERT (false);
    }
  return size;
}

void
GUSearchMessage::Print (std::ostream &os) const
{
  os << "\n****GUSearchMessage Dump****\n" ;
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
      default:
        break;  
    }
  os << "\n****END OF MESSAGE****\n";
}

void
GUSearchMessage::Serialize (Buffer::Iterator start) const
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
      case PUBLISH_REQ:
        m_message.publishReq.Serialize(i);
        break;
      case SEARCH_REQ:
        m_message.searchReq.Serialize(i);
        break;
      case SEARCH_RSP:
        m_message.searchRsp.Serialize(i);
        break;
      default:
        NS_ASSERT (false);   
    }
}

uint32_t 
GUSearchMessage::Deserialize (Buffer::Iterator start)
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
      case PUBLISH_REQ:
        m_message.publishReq.Deserialize(i);
        break;
      case SEARCH_REQ:
        m_message.searchReq.Deserialize(i);
        break;
      case SEARCH_RSP:
        m_message.searchRsp.Deserialize(i);
        break;
      default:
        NS_ASSERT (false);
    }
  return size;
}

/* PING_REQ */

uint32_t 
GUSearchMessage::PingReq::GetSerializedSize (void) const
{
  uint32_t size;
  size = sizeof(uint16_t) + pingMessage.length();
  return size;
}

void
GUSearchMessage::PingReq::Print (std::ostream &os) const
{
  os << "PingReq:: Message: " << pingMessage << "\n";
}

void
GUSearchMessage::PingReq::Serialize (Buffer::Iterator &start) const
{
  start.WriteU16 (pingMessage.length ());
  start.Write ((uint8_t *) (const_cast<char*> (pingMessage.c_str())), pingMessage.length());
}

uint32_t
GUSearchMessage::PingReq::Deserialize (Buffer::Iterator &start)
{  
  uint16_t length = start.ReadU16 ();
  char* str = (char*) malloc (length);
  start.Read ((uint8_t*)str, length);
  pingMessage = std::string (str, length);
  free (str);
  return PingReq::GetSerializedSize ();
}

void
GUSearchMessage::SetPingReq (std::string pingMessage)
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

GUSearchMessage::PingReq
GUSearchMessage::GetPingReq ()
{
  return m_message.pingReq;
}

/* PING_RSP */

uint32_t 
GUSearchMessage::PingRsp::GetSerializedSize (void) const
{
  uint32_t size;
  size = sizeof(uint16_t) + pingMessage.length();
  return size;
}

void
GUSearchMessage::PingRsp::Print (std::ostream &os) const
{
  os << "PingReq:: Message: " << pingMessage << "\n";
}

void
GUSearchMessage::PingRsp::Serialize (Buffer::Iterator &start) const
{
  start.WriteU16 (pingMessage.length ());
  start.Write ((uint8_t *) (const_cast<char*> (pingMessage.c_str())), pingMessage.length());
}

uint32_t
GUSearchMessage::PingRsp::Deserialize (Buffer::Iterator &start)
{  
  uint16_t length = start.ReadU16 ();
  char* str = (char*) malloc (length);
  start.Read ((uint8_t*)str, length);
  pingMessage = std::string (str, length);
  free (str);
  return PingRsp::GetSerializedSize ();
}

void
GUSearchMessage::SetPingRsp (std::string pingMessage)
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

GUSearchMessage::PingRsp
GUSearchMessage::GetPingRsp ()
{
  return m_message.pingRsp;
}

void
GUSearchMessage::PublishReq::Print(std::ostream &os) const {
  os << "PublishReq::for keyword: " << keyword << "and documents:";
  for(uint32_t i =0; i < docList.size(); i++) {
    os << " " << docList[i];
  }
  os << "\n";
}

uint32_t
GUSearchMessage::PublishReq::GetSerializedSize(void) const {
  uint32_t size = 0;
  size+= sizeof(uint16_t) + keyword.length(); //store size of keyword and keyword
  size += sizeof(uint32_t);//store a number that tells us how many bytes of vector
  for(uint32_t i =0; i < docList.size(); i++) {//store vector
    size += sizeof(uint16_t) + docList[i].length();
  }
  return size;
}

void
GUSearchMessage::PublishReq::Serialize(Buffer::Iterator &start) const {

  //write keyword
  start.WriteU16(keyword.length());
  start.Write((uint8_t *) (const_cast<char*> (keyword.c_str())), keyword.length());
  
  //write vector size including vector element sizes
  start.WriteU16(docList.size());

  //write vector
  for(uint32_t i = 0; i < docList.size(); i++) {
    std::string docName = docList[i];
    start.WriteU16(docName.length());
    start.Write((uint8_t *) (const_cast<char*> (docName.c_str())), docName.length());
  }
}

uint32_t
GUSearchMessage::PublishReq::Deserialize (Buffer::Iterator &start) {
  //read keyWord
  uint16_t length = start.ReadU16();
  char* str = (char*) malloc (length);
  start.Read((uint8_t*)str, length);
  keyword = std::string(str, length);
  length = 0;
  free(str);

  uint16_t vectorLength = start.ReadU16();
  //read vector
  for(uint16_t i = 0; i < vectorLength; i++) {
    length = start.ReadU16();
    str = (char*) malloc (length);
    start.Read((uint8_t*)str, length);
    docList.push_back(std::string(str,length));
    length = 0;
    free(str);
  }
  return PublishReq::GetSerializedSize();
}

GUSearchMessage::PublishReq
GUSearchMessage::getPublishReq() {
  return m_message.publishReq;
}

void
GUSearchMessage::setPublishReq(std::string keyword, std::vector<std::string> docList) {
  if(m_messageType == 0)
    m_messageType = PUBLISH_REQ;
  else 
    NS_ASSERT(m_messageType == PUBLISH_REQ);

  m_message.publishReq.keyword = keyword;
  m_message.publishReq.docList = docList;
}

void 
GUSearchMessage::SearchReq::Print(std::ostream &os) const {
  os << "SearchReq::from " << originatorAddr << "to " << destinationAddress << " for keywords: ";
  for(uint i = 0; i < keywords.size(); i++) {
    os << keywords[i];
    if(i != keywords.size() -1)
      os << " AND";
  }
  os << "\n";
}

uint32_t 
GUSearchMessage::SearchReq::GetSerializedSize (void) const {
  uint32_t size = 0;
  size += sizeof(uint32_t); //originatoraddr
  size += sizeof(uint32_t); //dest addr
  size += sizeof(uint32_t); //originator's transactionID
  size += sizeof(uint16_t); //keywords vector size
  for(uint32_t i = 0; i < keywords.size(); i++) {
    size+= sizeof(uint16_t); //to store keyword length
    size += keywords[i].length(); //size of actual keyword
  }
  size += sizeof(uint16_t); //doclist vector size
  for(uint32_t i = 0; i < docList.size(); i++) {
    size += sizeof(uint16_t); //store docname length
    size += docList[i].length();
  }
  
  return size;
}

void 
GUSearchMessage::SearchReq::Serialize (Buffer::Iterator &start) const{
  //write originator addr
  start.WriteU32(originatorAddr);
  //write dest addr
  start.WriteU32(destinationAddress);
  //write originator's transaction ID
  start.WriteU32(originatorTransactionID);
  //write keywords vector size
  start.WriteU16(keywords.size());
  //write keywords vector
  for(uint32_t i = 0; i < keywords.size(); i++) {
    std::string keyword = keywords[i];
    start.WriteU16(keyword.length());
    start.Write((uint8_t *) (const_cast<char*> (keyword.c_str())), keyword.length());
  }
  //write docList vector size
  start.WriteU16(docList.size());
  //write docList vector
  for(uint32_t i = 0; i < docList.size(); i++) {
    std::string docname = docList[i];
    start.WriteU16(docname.length());
    start.Write((uint8_t *) (const_cast<char*> (docname.c_str())), docname.length());
  }
}

uint32_t 
GUSearchMessage::SearchReq::Deserialize (Buffer::Iterator &start) {
  //read originator addr
  originatorAddr = start.ReadU32();
  //read dest addr
  destinationAddress = start.ReadU32();
  //read originator's transaction ID
  originatorTransactionID = start.ReadU32();
  //read keywords vector size
  uint16_t keywords_size = start.ReadU16();
  //read keyword vector
  char* str;
  uint16_t length = 0;
  for(uint16_t i = 0; i < keywords_size; i++) {
    length = start.ReadU16();
    str = (char*) malloc (length);
    start.Read((uint8_t*)str, length);
    keywords.push_back(std::string(str,length));
    length = 0;
    free(str);
  }
  //read docList vector size
  uint16_t docList_size = start.ReadU16();
  //read docList vector
  for(uint16_t i = 0; i < docList_size; i++) {
    length = start.ReadU16();
    str = (char*) malloc (length);
    start.Read((uint8_t*)str, length);
    docList.push_back(std::string(str,length));
    length = 0;
    free(str);
  }
  return SearchReq::GetSerializedSize();
}

GUSearchMessage::SearchReq
GUSearchMessage::getSearchReq() {
  return m_message.searchReq;
}

void 
GUSearchMessage::setSearchReq(std::vector<std::string> keywords, std::vector<std::string> docList, 
uint32_t originatorAddr, uint32_t destinationAddress, uint32_t originatorTransactionID) {
  if(m_messageType == 0)
    m_messageType = SEARCH_REQ;
  else 
    NS_ASSERT(m_messageType == SEARCH_REQ);

  m_message.searchReq.keywords = keywords;
  m_message.searchReq.docList = docList;
  m_message.searchReq.originatorAddr = originatorAddr;
  m_message.searchReq.destinationAddress = destinationAddress;
  m_message.searchReq.originatorTransactionID = originatorTransactionID;
}


void 
GUSearchMessage::SearchRsp::Print(std::ostream &os) const {
  os << "SearchRsp::for search #" << originatorTransactionID << "with documents:";
  for(uint32_t i =0; i < docList.size(); i++) {
    os << " " << docList[i];
  }
  os << "\n";
}

uint32_t 
GUSearchMessage::SearchRsp::GetSerializedSize (void) const {
  uint32_t size = 0;
  size += sizeof(uint32_t); //originator transaction id;
  size += sizeof(uint32_t); //length of docList;
  for(uint16_t i =0; i < docList.size(); i++) {
    size += sizeof(uint16_t); //length of doc name
    size += docList[i].length(); //size of doc name
  }
  return size;
}

void 
GUSearchMessage::SearchRsp::Serialize (Buffer::Iterator &start) const {
  start.WriteU32(originatorTransactionID); //write originator transaction ID
  start.WriteU32(docList.size()); //write length
  for(uint16_t i =0; i < docList.size(); i++) {
    std::string docname = docList[i]; //write docname length
    start.WriteU16(docname.length()); //write docname
    start.Write((uint8_t *) (const_cast<char*> (docname.c_str())), docname.length());
  }
}

uint32_t 
GUSearchMessage::SearchRsp::Deserialize (Buffer::Iterator &start) {

  originatorTransactionID = start.ReadU32();
  //read doclist vector size
  uint32_t doclist_size = start.ReadU32();
  //read doclist vector
  uint16_t length = 0;
  char* str;
  for(uint32_t i = 0; i < doclist_size; i++) {
    length = start.ReadU16();
    str = (char*) malloc (length);
    start.Read((uint8_t*)str, length);
    docList.push_back(std::string(str,length));
    length = 0;
    free(str);
  }
  return SearchRsp::GetSerializedSize();
}

GUSearchMessage::SearchRsp 
GUSearchMessage::getSearchRsp() {
  return m_message.searchRsp;
}

void 
GUSearchMessage::setSearchRsp(std::vector<std::string> docList, uint32_t originatorTransactionID) {
  if(m_messageType == 0)
    m_messageType = SEARCH_RSP;
  else 
    NS_ASSERT(m_messageType == SEARCH_RSP);

  m_message.searchRsp.originatorTransactionID = originatorTransactionID;
  m_message.searchRsp.docList = docList;
}

void
GUSearchMessage::SetMessageType (MessageType messageType)
{
  m_messageType = messageType;
}

GUSearchMessage::MessageType
GUSearchMessage::GetMessageType () const
{
  return m_messageType;
}

void
GUSearchMessage::SetTransactionId (uint32_t transactionId)
{
  m_transactionId = transactionId;
}

uint32_t 
GUSearchMessage::GetTransactionId (void) const
{
  return m_transactionId;
}