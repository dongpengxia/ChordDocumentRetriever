#include "gu-search.h"
#include "ns3/random-variable.h"
#include "ns3/inet-socket-address.h"

using namespace ns3;

TypeId
GUSearch::GetTypeId ()
{
  static TypeId tid = TypeId ("GUSearch")
    .SetParent<GUApplication> ()
    .AddConstructor<GUSearch> ()
    .AddAttribute ("AppPort",
                   "Listening port for Application",
                   UintegerValue (10000),
                   MakeUintegerAccessor (&GUSearch::m_appPort),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("ChordPort",
                   "Listening port for Application",
                   UintegerValue (10001),
                   MakeUintegerAccessor (&GUSearch::m_chordPort),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("PingTimeout",
                   "Timeout value for PING_REQ in milliseconds",
                   TimeValue (MilliSeconds (2000)),
                   MakeTimeAccessor (&GUSearch::m_pingTimeout),
                   MakeTimeChecker ())
    ;
  return tid;
}

GUSearch::GUSearch ()
  : m_auditPingsTimer (Timer::CANCEL_ON_DESTROY)
{
  m_chord = NULL;
  RandomVariable random;
  SeedManager::SetSeed (time (NULL));
  random = UniformVariable (0x00000000, 0xFFFFFFFF);
  m_currentTransactionId = random.GetInteger ();
}

GUSearch::~GUSearch ()
{

}

void
GUSearch::DoDispose ()
{
  StopApplication ();
  GUApplication::DoDispose ();
}

void
GUSearch::StartApplication (void)
{
  // Create and Configure GUChord
  ObjectFactory factory;
  factory.SetTypeId (GUChord::GetTypeId ());
  factory.Set ("AppPort", UintegerValue (m_chordPort));
  m_chord = factory.Create<GUChord> ();
  m_chord->SetNode (GetNode ());
  m_chord->SetNodeAddressMap (m_nodeAddressMap);
  m_chord->SetAddressNodeMap (m_addressNodeMap);
  m_chord->SetModuleName ("CHORD");
  std::string nodeId = GetNodeId ();
  m_chord->SetNodeId (nodeId);
  m_chord->SetLocalAddress(m_local);

  if (GUApplication::IsRealStack ())
  {
    m_chord->SetRealStack (true);
  } 

  // Configure Callbacks with Chord
  m_chord->SetPingSuccessCallback (MakeCallback (&GUSearch::HandleChordPingSuccess, this)); 
  m_chord->SetPingFailureCallback (MakeCallback (&GUSearch::HandleChordPingFailure, this));
  m_chord->SetPingRecvCallback (MakeCallback (&GUSearch::HandleChordPingRecv, this)); 
  m_chord->SetLookupCallback(MakeCallback (&GUSearch::HandleChordLookup, this));
  m_chord->SetRedistributeKeysCallback(MakeCallback(&GUSearch::HandleChordRedistribute, this));
  // Start Chord
  m_chord->SetStartTime (Simulator::Now());
  m_chord->Start ();
  if (m_socket == 0)
    { 
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny(), m_appPort);
      m_socket->Bind (local);
      m_socket->SetRecvCallback (MakeCallback (&GUSearch::RecvMessage, this));
    }  
  
  // Configure timers
  m_auditPingsTimer.SetFunction (&GUSearch::AuditPings, this);
  // Start timers
  m_auditPingsTimer.Schedule (m_pingTimeout);
}

void
GUSearch::StopApplication (void)
{
  //Stop chord
  m_chord->StopChord ();
  // Close socket
  if (m_socket)
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
      m_socket = 0;
    }

  // Cancel timers
  m_auditPingsTimer.Cancel ();
  m_pingTracker.clear ();
}

void
GUSearch::ProcessCommand (std::vector<std::string> tokens)
{
  std::vector<std::string>::iterator iterator = tokens.begin();
  std::string command = *iterator;
  if (command == "CHORD")
    { 
      // Send to Chord Sub-Layer
      tokens.erase (iterator);
      m_chord->ProcessCommand (tokens);
    } 
  if (command == "PING")
    {
      if (tokens.size() < 3)
        {
          ERROR_LOG ("Insufficient PING params..."); 
          return;
        }
      iterator++;
      if (*iterator != "*")
        {
          std::string nodeId = *iterator;
          iterator++;
          std::string pingMessage = *iterator;
          SendPing (nodeId, pingMessage);
        }
      else
        {
          iterator++;
          std::string pingMessage = *iterator;
          std::map<uint32_t, Ipv4Address>::iterator iter;
          for (iter = m_nodeAddressMap.begin () ; iter != m_nodeAddressMap.end (); iter++)  
            {
              std::ostringstream sin;
              uint32_t nodeNumber = iter->first;
              sin << nodeNumber;
              std::string nodeId = sin.str();    
              SendPing (nodeId, pingMessage);
            }
        }
    }

  if(command == "PUBLISH") {
    iterator++;
    std::string filename = *iterator;
    StartPublish(filename);
  }
  if(command == "DOCS") {
    SEARCH_LOG("PRINTING DOCUMENT INFORMATION");
    std::map<std::string, std::vector<std::string> >::iterator it;
    for(it = m_invertedTable.begin(); it != m_invertedTable.end(); ++it) {
      std::string keyword = it->first;
      std::vector<std::string> doclist = it->second;
      std::cout << "keyword: " << keyword << " hashed to " << stringHash(keyword);
      std::cout << " docs: <";
      for(uint32_t i = 0; i < doclist.size(); i++) {
        std::cout << doclist[i];
        if(i != doclist.size() -1) {
          std::cout << ", ";
        }
      }
      std::cout << ">" <<std::endl;
    }
  }
  if(command == "ORDER") {
    iterator++;
    std::string testVal = *iterator;
    uint32_t val = atoi(testVal.c_str());
    if(m_chord->isOwn(val)) {
      SEARCH_LOG("Key: " << val << " belongs to us")
    }
    else {
      SEARCH_LOG("Key: " << val << " DOES NOT belong to us ")
    }
  }
  if(command == "SEARCH") {
    iterator++;
    std::string nodeNumberStr = *iterator;
    for(uint32_t i = 0; i < nodeNumberStr.length(); i++) {
      if(nodeNumberStr[i] > '9' || nodeNumberStr[i] < '0') {
        SEARCH_LOG("Error- invalid node number");
        return;
      }
    }
    uint32_t nodeNumber = atoi(nodeNumberStr.c_str());
    iterator++;
    std::vector<std::string> keywords;
    for(std::vector<std::string>::iterator it = iterator; it != tokens.end(); it++) {
      keywords.push_back(*it);
    }
    if(keywords.size() == 0) {
      SEARCH_LOG("Error - no keywords entered");
      return;
    }
    StartSearch(nodeNumber, keywords);
  }
}

void
GUSearch::SendPing (std::string nodeId, std::string pingMessage)
{
  // Send Ping Via-Chord layer 
  SEARCH_LOG ("Sending Ping via Chord Layer to node: " << nodeId << " Message: " << pingMessage);
  Ipv4Address destAddress = ResolveNodeIpAddress(nodeId);
  m_chord->SendPing (destAddress, pingMessage);
}

void
GUSearch::SendGUSearchPing (Ipv4Address destAddress, std::string pingMessage)
{
  if (destAddress != Ipv4Address::GetAny ())
    {
      uint32_t transactionId = GetNextTransactionId ();
      SEARCH_LOG ("Sending PING_REQ to Node: " << ReverseLookup(destAddress) << " IP: " << destAddress << " Message: " << pingMessage << " transactionId: " << transactionId);
      Ptr<PingRequest> pingRequest = Create<PingRequest> (transactionId, Simulator::Now(), destAddress, pingMessage);
      // Add to ping-tracker
      m_pingTracker.insert (std::make_pair (transactionId, pingRequest));
      Ptr<Packet> packet = Create<Packet> ();
      GUSearchMessage message = GUSearchMessage (GUSearchMessage::PING_REQ, transactionId);
      message.SetPingReq (pingMessage);
      packet->AddHeader (message);
      m_socket->SendTo (packet, 0 , InetSocketAddress (destAddress, m_appPort));
    }
}

void
GUSearch::RecvMessage (Ptr<Socket> socket)
{
  Address sourceAddr;
  Ptr<Packet> packet = socket->RecvFrom (sourceAddr);
  InetSocketAddress inetSocketAddr = InetSocketAddress::ConvertFrom (sourceAddr);
  Ipv4Address sourceAddress = inetSocketAddr.GetIpv4 ();
  uint16_t sourcePort = inetSocketAddr.GetPort ();
  GUSearchMessage message;
  packet->RemoveHeader (message);

  switch (message.GetMessageType ())
    {
      case GUSearchMessage::PING_REQ:
        ProcessPingReq (message, sourceAddress, sourcePort);
        break;
      case GUSearchMessage::PING_RSP:
        ProcessPingRsp (message, sourceAddress, sourcePort);
        break;
      case GUSearchMessage::PUBLISH_REQ:
        ProcessPublishReq(message, sourceAddress, sourcePort);
        break;
      case GUSearchMessage::SEARCH_REQ:
        ProcessSearchReq(message, sourceAddress, sourcePort);
        break;
      case GUSearchMessage::SEARCH_RSP:
        ProcessSearchRsp(message, sourceAddress, sourcePort);
        break;
      default:
        ERROR_LOG ("Unknown Message Type!");
        break;
    }
}

void
GUSearch::ProcessPingReq (GUSearchMessage message, Ipv4Address sourceAddress, uint16_t sourcePort)
{
    // Use reverse lookup for ease of debug
    std::string fromNode = ReverseLookup (sourceAddress);
    SEARCH_LOG ("Received PING_REQ, From Node: " << fromNode << ", Message: " << message.GetPingReq().pingMessage);
    // Send Ping Response
    GUSearchMessage resp = GUSearchMessage (GUSearchMessage::PING_RSP, message.GetTransactionId());
    resp.SetPingRsp (message.GetPingReq().pingMessage);
    Ptr<Packet> packet = Create<Packet> ();
    packet->AddHeader (resp);
    m_socket->SendTo (packet, 0 , InetSocketAddress (sourceAddress, sourcePort));
}

void
GUSearch::ProcessPingRsp (GUSearchMessage message, Ipv4Address sourceAddress, uint16_t sourcePort)
{
  // Remove from pingTracker
  std::map<uint32_t, Ptr<PingRequest> >::iterator iter;
  iter = m_pingTracker.find (message.GetTransactionId ());
  if (iter != m_pingTracker.end ())
    {
      std::string fromNode = ReverseLookup (sourceAddress);
      SEARCH_LOG ("Received PING_RSP, From Node: " << fromNode << ", Message: " << message.GetPingRsp().pingMessage);
      m_pingTracker.erase (iter);
    }
  else
    {
      DEBUG_LOG ("Received invalid PING_RSP!");
    }
}

uint32_t
GUSearch::stringHash(std::string str) {
  unsigned char digest[SHA_DIGEST_LENGTH] = {0};
  SHA1((unsigned char*)str.c_str(), str.size(), (unsigned char*)&digest);

  uint64_t truncatedHash = 0;
  for(int i = 0; i < SHA_DIGEST_LENGTH; i++) {
      truncatedHash = (truncatedHash << 8) + int(digest[i]);
  }

  return truncatedHash % m_chord->getNumRingSlots();
}

void GUSearch::HandleChordLookup(Ipv4Address destAddress, uint32_t transactionID) {
  // SEARCH_LOG("Chord Layer Received Lookup! Source nodeID: "<< ReverseLookup(destAddress) << " IP: " << destAddress << " transactionID: " << transactionID);
  std::map<uint32_t, PublishEvent>::iterator it = m_pendingPublish.find(transactionID);
  if(it != m_pendingPublish.end()) {
    PublishEvent publishingEvent= it->second;
    SendPublishReq(destAddress, publishingEvent.keyword, publishingEvent.docList);
    m_pendingPublish.erase(it);
  }
  else {
    std::map<uint32_t, SearchEvent>::iterator iter = m_pendingSearch.find(transactionID);
    if(iter != m_pendingSearch.end()) {
      SearchEvent searchingEvent = iter->second;

      SendSearchReq(destAddress, searchingEvent.originatorAddr, searchingEvent.intermediateNode, 
          searchingEvent.keywords, searchingEvent.localDocs, searchingEvent.originatorTransactionID);
    }
    else {
      SEARCH_LOG("Unable to find matching event for transactionID: " << transactionID);
    }
  }
}

void
GUSearch::HandleChordRedistribute(uint8_t isPredecessor, Ipv4Address addrIP, uint32_t predecessorKey) {
  if(isPredecessor) {
    SEARCH_LOG("Chord Layer has new Predecessor: " << addrIP << ", redistributing keys less than " << predecessorKey);
    std::map<std::string, std::vector<std::string> >::iterator it;
    for(it = m_invertedTable.begin(); it != m_invertedTable.end();) {
      if(stringHash(it->first) <= predecessorKey) {
        SendPublishReq(addrIP, it->first, it->second);
        m_invertedTable.erase(it++);
      }
      else {
        ++it;
      }
    }
  }
  else {
    //our node is leaving and this is Chord's way of letting us know
    //give all our keys to our successor
    SEARCH_LOG("Leaving Chord, redistributing all our keys");
    std::map<std::string, std::vector<std::string> >::iterator it;
    for(it = m_invertedTable.begin(); it != m_invertedTable.end();) {
      SendPublishReq(addrIP, it->first, it->second);
      m_invertedTable.erase(it++);
    }
  }
}

void 
GUSearch::StartPublish(std::string filename) {
  std::map<std::string, std::vector<std::string> > terms;
  std::ifstream f(filename.c_str());
  if(!f.is_open()) {
      SEARCH_LOG("ERROR - could not open file" << filename);
      return;
  }
  std::string str;
  while (std::getline(f, str)) {
    if(str.size() != 0) {
      std::stringstream ss(str);
      std::string item;
      //handle doc name
      std::getline(ss, item, ' ');
      if(item[item.length()-1] == '\r') {
        item.erase(item.length()-1);
      }
      std::string docname = item;
      //handle terms
      while(std::getline(ss, item, ' ')) {
        if(item[item.length()-1] == '\r') {
          item.erase(item.length()-1);
        }
        
        if(terms.find(item) != terms.end()){
          terms[item].push_back(docname);
        }
        else {
          std::vector<std::string> docList;
          docList.push_back(docname);
          terms[item] = docList;
        }
      }
    }
  }
  f.close();
  std::map<std::string, std::vector<std::string> >::iterator it;
  for(it = terms.begin(); it != terms.end(); it++) {
      ProcessPublish(it->first, it->second);
  }
}

// HANDLE PUBLISH_REQ
void
GUSearch::ProcessPublish(std::string keyword, std::vector<std::string> docList) {
  uint32_t ahashedKey = stringHash(keyword);
  // SEARCH_LOG("PUBLISHING documents for keyword: " << keyword << " hashed to : " << ahashedKey);
  std::stringstream ss;
  for(uint32_t i = 0; i < docList.size(); i++) {
    ss << docList[i];
    if(i != docList.size()-1) {
      ss << ", ";
    }
  }
  SEARCH_LOG("PUBLISH <" << keyword << ", " << ss.str() << ">");

  //check if we should save this locally
  if(m_chord->isOwn(ahashedKey)) {
    storeInvertedTableEntry(keyword, docList);
  }
  else {
    PublishEvent newEvent;
    uint32_t nextTransactionID = GetNextTransactionId();
    newEvent.keyword = keyword;
    newEvent.docList = docList;
    newEvent.hashedKey = ahashedKey;

    m_pendingPublish.insert (std::make_pair (nextTransactionID, newEvent));

    m_chord->GUSearchLookup(nextTransactionID, newEvent.hashedKey);
  }
}

void 
GUSearch::SendPublishReq(Ipv4Address destAddress, std::string keyword, std::vector<std::string> docList) {
  if (destAddress != Ipv4Address::GetAny ())
    {
      uint32_t transactionId = GetNextTransactionId ();
      // SEARCH_LOG ("Sending PUBLISHREQ to Node: " << ReverseLookup(destAddress) << " Keyword: " << keyword << " hashed keyword: " << stringHash(keyword) << " transactionId: " << transactionId);
      std::stringstream ss;
      for(uint32_t i = 0; i < docList.size(); i++) {
        ss << docList[i];
        if(i != docList.size()-1) {
          ss << ", ";
        }
      }
      SEARCH_LOG("InvertedListShip <" << keyword << ", " << ss.str() << ">");
      Ptr<Packet> packet = Create<Packet>();
      GUSearchMessage message = GUSearchMessage (GUSearchMessage::PUBLISH_REQ, transactionId);
      message.setPublishReq(keyword, docList);
      packet->AddHeader (message);
      m_socket->SendTo (packet, 0 , InetSocketAddress (destAddress, m_appPort));
    }
}

void 
GUSearch::ProcessPublishReq(GUSearchMessage message, Ipv4Address sourceAddress, uint16_t sourcePort) {

  std::string keyword = message.getPublishReq().keyword;
  std::vector<std::string> docList = message.getPublishReq().docList;
  std::string fromNode = ReverseLookup (sourceAddress);
  // SEARCH_LOG ("Received PUBLISH_REQ, From Node: " << fromNode << " with key: " << keyword << " hashed to " << stringHash(keyword));

  //save our mapping
  storeInvertedTableEntry(keyword, docList);
}

void
GUSearch::storeInvertedTableEntry(std::string keyword, std::vector<std::string> docList) {

  std::map<std::string, std::vector<std::string> >::iterator it;
  it = m_invertedTable.find(keyword);
    std::stringstream ss;
  for(uint32_t i = 0; i < docList.size(); i++) {
    ss << docList[i];
    if(i != docList.size()-1) {
      ss << ", ";
    }
  }
  SEARCH_LOG("Store <" << keyword << ", " << ss.str() << ">");
  //check if we have some docs for the key already
  if(it != m_invertedTable.end()) {
    std::vector<std::string> docs = it->second;
    std::set<std::string> s;
    for(uint32_t i = 0; i < docs.size(); i++) s.insert(docs[i]);
    for(uint32_t i = 0; i < docList.size(); i++) s.insert(docList[i]);
    docs.assign(s.begin(),s.end());
    it->second = docs;
  }
  else {
    m_invertedTable.insert (std::make_pair (keyword, docList));
  }
}

// HANDLE SEARCH_REQ
std::vector<std::string>
GUSearch::intersection(std::vector<std::string> v1, std::vector<std::string> v2) {
  std::vector<std::string> toReturn;

  std::sort(v1.begin(), v1.end());
  std::sort(v2.begin(), v2.end());

  std::set_intersection(v1.begin(),v1.end(),v2.begin(),v2.end(),std::back_inserter(toReturn));
  return toReturn;

}

std::vector<std::string>
GUSearch::findDocs(std::vector<std::string> &keywords) {
  std::vector<std::string> foundDocs;
  std::vector<std::string> remainingKeyWords;

  for(uint32_t i = 0; i < keywords.size(); i++) {
    uint32_t hashedKey = stringHash(keywords[i]);
    if(m_chord->isOwn(hashedKey)) {
      std::vector<std::string> currentSearch;
      std::map<std::string, std::vector<std::string> >::iterator it;
      it = m_invertedTable.find(keywords[i]);
  
      if(it != m_invertedTable.end()) {
        currentSearch = it->second;
        if(foundDocs.empty())
          foundDocs = currentSearch;
        else
          foundDocs = intersection(foundDocs, currentSearch);
      }
      else {
        foundDocs.clear(); //intersection in this case is empty
      }
    }
    else {
      remainingKeyWords.push_back(keywords[i]);
    }
  }
  keywords = remainingKeyWords;
  return foundDocs;
}

void
GUSearch::StartSearch(uint32_t nodeNumber, std::vector<std::string> keywords) {
  std::vector<std::string> docs;
  docs.clear();
  uint32_t originatorTransactionID = GetNextTransactionId();
  Ipv4Address dest = m_nodeAddressMap[nodeNumber];
  // SEARCH_LOG("Starting search #" << originatorTransactionID);
  std::stringstream ss;
  for(uint32_t i = 0; i < keywords.size(); i++) {
    ss << keywords[i];
    if(i != keywords.size()-1) {
      ss << ", ";
    }
  }
  SEARCH_LOG("Search<" << ss.str() << ">");
  if(dest != m_local) {
    // send to nodeNumber
    SearchEvent newEvent;
    newEvent.originalkeys = keywords;
    newEvent.keywords = keywords;
    newEvent.localDocs = docs;
    newEvent.intermediateNode = nodeNumber;
    newEvent.originatorAddr = m_chord->getLocalAddr();
    newEvent.originatorTransactionID = originatorTransactionID;
    m_pendingSearch.insert (std::make_pair (originatorTransactionID, newEvent));

    SendSearchReq(dest, m_chord->getLocalAddr(), nodeNumber, keywords, docs, originatorTransactionID);
  }
  else {
    //otherwise ignore the nodeNumber and proceed directly
    ProcessSearch(nodeNumber, m_chord->getLocalAddr(), originatorTransactionID, keywords, docs);
  }
}

void 
GUSearch::ProcessSearch(uint32_t nodeNumber, uint32_t originatorAddr, uint32_t originatorTransactionID,
  std::vector<std::string> keywords, std::vector<std::string> docs) {

  // SEARCH_LOG("ProcessSearch from " << originatorAddr << " #" << originatorTransactionID);

  uint32_t originalKeywordsSize = keywords.size();
  std::vector<std::string> originalKeywords = keywords;

  // search locally
  std::vector<std::string> localResults;
  localResults = findDocs(keywords);

  if(!docs.empty()) {
    localResults = intersection(localResults, docs);
  }

  //debug help
  std::stringstream ss;
  for(uint32_t i = 0; i < localResults.size(); i++) {
    ss << localResults[i];
    if(i != localResults.size()-1) {
      ss << ", ";
    }
  }
  std::stringstream os;
  for(uint32_t i = 0; i < keywords.size(); i++) {
    os << keywords[i];
    if(i != keywords.size()-1) {
      os << ", ";
    }
  }
  SEARCH_LOG("Remaining keywords to search <" << os.str() << ">\nCurrentResults<" << ss.str() << ">");

  SearchEvent newEvent;
  newEvent.originalkeys = originalKeywords;
  newEvent.keywords = keywords;
  newEvent.localDocs = localResults;
  newEvent.intermediateNode = nodeNumber;
  newEvent.originatorAddr = originatorAddr;
  newEvent.originatorTransactionID = originatorTransactionID;
  m_pendingSearch.insert (std::make_pair (originatorTransactionID, newEvent));

  if ((m_chord->isAlone() && localResults.empty()) || (m_chord->isAlone() && !keywords.empty())) {
    //case: we're alone and we can't complete the search for some reason
    // SEARCH_LOG("Local search #" << originatorTransactionID << " unsuccessful, NO RESULTS");
    SEARCH_LOG("SearchResults<" << nodeNumber << ", Empty List>")
    SendSearchRsp(originatorAddr, originatorTransactionID, localResults);
  }
  else if(m_chord->isAlone() && keywords.empty() && !localResults.empty()){
    //case: we have succesfully finished the lookup with a single node in the ring
    // SEARCH_LOG("Local search #" << originatorTransactionID << " SUCCESSFUL");
    std::stringstream ss;
    for(uint32_t i = 0; i < localResults.size(); i++) {
      ss << localResults[i];
      if(i != localResults.size()-1) {
        ss << ", ";
      }
    }
    SEARCH_LOG("SearchResults<" << nodeNumber << ", " << ss.str() << ">");
    SendSearchRsp(originatorAddr, originatorTransactionID, localResults);
  }
  else if((!keywords.empty() && !localResults.empty()) || (keywords.size() == originalKeywordsSize)) {
    //case: found some results and still have more to find
    //OR none of the keywords can be found at our node ( in the case that this is local)
    //hash first key and pass the REQ to them
    uint32_t hashedKey = stringHash(keywords[0]);
    // SEARCH_LOG("SearchReq: performing lookup on Chord Layer for search #" << originatorTransactionID);
    m_chord->GUSearchLookup(originatorTransactionID, hashedKey);
  }
  else if(keywords.empty() && !localResults.empty()) {
    //case: found everything, send to originator 
    // SEARCH_LOG("Search from: " << originatorAddr << " #" << originatorTransactionID << " successful");
    std::stringstream ss;
    for(uint32_t i = 0; i < localResults.size(); i++) {
      ss << localResults[i];
      if(i != localResults.size()-1) {
        ss << ", ";
      }
    }
    SEARCH_LOG("SearchResults<" << nodeNumber << ", " << ss.str() << ">");
    SendSearchRsp(originatorAddr, originatorTransactionID, localResults);
  }
  else if(!keywords.empty() && localResults.empty()){
    //the intersection of results is empty
    // SEARCH_LOG("Search from: " << originatorAddr << " for search #" << originatorTransactionID << " completed with NO RESULTS")
    SEARCH_LOG("SearchResults<" << nodeNumber << ", Empty List>")
    SendSearchRsp(originatorAddr, originatorTransactionID, localResults);
  }
  else {
    //case: keywords don't exist anywhere
    // SEARCH_LOG("Search from: " << originatorAddr << " for search #" << originatorTransactionID << " completed with NO RESULTS")
    SEARCH_LOG("SearchResults<" << nodeNumber << ", Empty List>")
    SendSearchRsp(originatorAddr, originatorTransactionID, localResults);
  }
}

void 
GUSearch::SendSearchReq(Ipv4Address destAddress, uint32_t originatorAddr, uint32_t intermediateAddr, 
  std::vector<std::string> keywords, std::vector<std::string> docList, uint32_t originatorTransactionID) {
  if (destAddress != Ipv4Address::GetAny ())
    {
      uint32_t transactionId = GetNextTransactionId ();
      // SEARCH_LOG ("Sending SEARCH_REQ to Node: " << ReverseLookup(destAddress) << " for search #" << originatorTransactionID);
      Ptr<Packet> packet = Create<Packet>();
      GUSearchMessage message = GUSearchMessage (GUSearchMessage::SEARCH_REQ, transactionId);
      message.setSearchReq(keywords, docList, originatorAddr, destAddress.Get(), originatorTransactionID);
      packet->AddHeader (message);
      m_socket->SendTo (packet, 0 , InetSocketAddress (destAddress, m_appPort));
    }
}

void
GUSearch::ProcessSearchReq(GUSearchMessage message, Ipv4Address sourceAddress, 
uint16_t sourcePort) {
  uint32_t nodeNumber = message.getSearchReq().destinationAddress;
  uint32_t originatorAddr = message.getSearchReq().originatorAddr;
  uint32_t originatorTransactionID = message.getSearchReq().originatorTransactionID;
  std::vector<std::string> keywords = message.getSearchReq().keywords;
  std::vector<std::string> docs = message.getSearchReq().docList;
  // SEARCH_LOG("Received SEARCH_REQ from " << sourceAddress << " for search #" << originatorTransactionID);
  ProcessSearch(nodeNumber, originatorAddr, originatorTransactionID, keywords, docs);
}

//handle search responses
void 
GUSearch::SendSearchRsp(uint32_t destAddress, uint32_t originatorTransactionID,
 std::vector<std::string> docList) {
  if(destAddress == m_chord->getLocalAddr()) {
    // SEARCH_LOG("Completed search #" << originatorTransactionID << " Locally with " << docList.size() << " results");
    PrintSearchResults(originatorTransactionID, docList);
  }
  else {
    
    if (Ipv4Address(destAddress) != Ipv4Address::GetAny ())
    {
      uint32_t transactionId = GetNextTransactionId ();
      // SEARCH_LOG("Sending SEARCH_RSP to: " << destAddress << " for search #" << originatorTransactionID << " with " << docList.size() << " results");
      Ptr<Packet> packet = Create<Packet>();
      GUSearchMessage message = GUSearchMessage (GUSearchMessage::SEARCH_RSP, transactionId);
      message.setSearchRsp(docList, originatorTransactionID);
      packet->AddHeader (message);
      m_socket->SendTo (packet, 0 , InetSocketAddress (Ipv4Address(destAddress), m_appPort));
    }
  }
  
  
}

void
GUSearch::ProcessSearchRsp(GUSearchMessage message, Ipv4Address sourceAddress, uint16_t sourcePort){
  uint32_t originatorTransactionID = message.getSearchRsp().originatorTransactionID;
  std::vector<std::string> docs = message.getSearchRsp().docList;
  // SEARCH_LOG("Received SEARCH_RSP from " << sourceAddress << " for search #" << originatorTransactionID);
  PrintSearchResults(originatorTransactionID, docs);
}

void GUSearch::PrintSearchResults(uint32_t originatorTransactionID, std::vector<std::string> docList) {
  SEARCH_LOG("RESULTS FOR SEARCH #" << originatorTransactionID)
  std::cout<< "==============================================" << std::endl;

  std::cout << "original search term(s) <";
  std::map<uint32_t, SearchEvent>::iterator it;
  it = m_pendingSearch.find(originatorTransactionID);
  if(it != m_pendingSearch.end()) {
    SearchEvent finishedEvent = it->second;
    
    std::vector<std::string> keywords = finishedEvent.originalkeys;
    for(uint32_t i = 0; i < keywords.size(); i++) {
      std::cout << keywords[i] << "<" << stringHash(keywords[i]) << ">";
      if( i != keywords.size() -1)
        std::cout << " AND ";
    }
    std::cout << ">" << std::endl;

    std::cout << "Documents found: <";
    for(uint32_t i = 0; i < docList.size(); i++) {
      std::cout << docList[i];
      if( i != docList.size() -1)
        std::cout << ", ";
    }
    if(docList.size() == 0) {
      std::cout << "NO RESULTS";
    }
    std::cout << ">" << std::endl;
  }
  else {
    SEARCH_LOG("ERROR: received search results for a search we can't find");
  }
}

void
GUSearch::AuditPings ()
{
  std::map<uint32_t, Ptr<PingRequest> >::iterator iter;
  for (iter = m_pingTracker.begin () ; iter != m_pingTracker.end();) {
      Ptr<PingRequest> pingRequest = iter->second;
      if (pingRequest->GetTimestamp().GetMilliSeconds() + m_pingTimeout.GetMilliSeconds() <= Simulator::Now().GetMilliSeconds()) {
          DEBUG_LOG ("Ping expired. Message: " << pingRequest->GetPingMessage () << " Timestamp: " << pingRequest->GetTimestamp().GetMilliSeconds () << " CurrentTime: " << Simulator::Now().GetMilliSeconds ());
          // Remove stale entries
          m_pingTracker.erase (iter++);
      }
      else {
          ++iter;
      }
  }
  // Reschedule timer
  m_auditPingsTimer.Schedule (m_pingTimeout); 
}

uint32_t
GUSearch::GetNextTransactionId ()
{
  return m_currentTransactionId++;
}

// Handle Chord Callbacks

void
GUSearch::HandleChordPingFailure (Ipv4Address destAddress, std::string message)
{
  SEARCH_LOG ("Chord Ping Expired! Destination nodeId: " << ReverseLookup(destAddress) << " IP: " << destAddress << " Message: " << message);
}

void
GUSearch::HandleChordPingSuccess (Ipv4Address destAddress, std::string message)
{
  SEARCH_LOG ("Chord Ping Success! Destination nodeId: " << ReverseLookup(destAddress) << " IP: " << destAddress << " Message: " << message);
  // Send ping via search layer 
  SendGUSearchPing (destAddress, message);
}

void
GUSearch::HandleChordPingRecv (Ipv4Address destAddress, std::string message)
{
  SEARCH_LOG ("Chord Layer Received Ping! Source nodeId: " << ReverseLookup(destAddress) << " IP: " << destAddress << " Message: " << message);
}

// Override GULog

void
GUSearch::SetTrafficVerbose (bool on)
{ 
  m_chord->SetTrafficVerbose (on);
  g_trafficVerbose = on;
}

void
GUSearch::SetErrorVerbose (bool on)
{ 
  m_chord->SetErrorVerbose (on);
  g_errorVerbose = on;
}

void
GUSearch::SetDebugVerbose (bool on)
{
  m_chord->SetDebugVerbose (on);
  g_debugVerbose = on;
}

void
GUSearch::SetStatusVerbose (bool on)
{
  m_chord->SetStatusVerbose (on);
  g_statusVerbose = on;
}

void
GUSearch::SetChordVerbose (bool on)
{
  m_chord->SetChordVerbose (on);
  g_chordVerbose = on;
}

void
GUSearch::SetSearchVerbose (bool on)
{
  m_chord->SetSearchVerbose (on);
  g_searchVerbose = on;
}