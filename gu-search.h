#ifndef GU_SEARCH_H
#define GU_SEARCH_H

#include "ns3/gu-application.h"
#include "ns3/gu-chord.h"
#include "ns3/gu-search-message.h"
#include "ns3/ping-request.h"
#include "ns3/ipv4-address.h"
#include <map>
#include <set>
#include <vector>
#include <string>
#include <openssl/sha.h>
#include <fstream>
#include "ns3/socket.h"
#include "ns3/nstime.h"
#include "ns3/timer.h"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"

using namespace ns3;

class GUSearch : public GUApplication
{
  public:
    static TypeId GetTypeId (void);
    GUSearch ();
    virtual ~GUSearch ();

    void SendPing (std::string nodeId, std::string pingMessage);
    void SendGUSearchPing (Ipv4Address destAddress, std::string pingMessage);
    void RecvMessage (Ptr<Socket> socket);
    void ProcessPingReq (GUSearchMessage message, Ipv4Address sourceAddress, uint16_t sourcePort);
    void ProcessPingRsp (GUSearchMessage message, Ipv4Address sourceAddress, uint16_t sourcePort);
    void AuditPings ();
    uint32_t GetNextTransactionId ();
	
    // Chord Callbacks
    void HandleChordPingSuccess (Ipv4Address destAddress, std::string message);
    void HandleChordPingFailure (Ipv4Address destAddress, std::string message);
    void HandleChordPingRecv (Ipv4Address destAddress, std::string message);

    // From GUApplication
    virtual void ProcessCommand (std::vector<std::string> tokens);
    // From GULog
    virtual void SetTrafficVerbose (bool on);
    virtual void SetErrorVerbose (bool on);
    virtual void SetDebugVerbose (bool on);
    virtual void SetStatusVerbose (bool on);
    virtual void SetChordVerbose (bool on);
    virtual void SetSearchVerbose (bool on);

    //hash strings using SHA1
    uint32_t stringHash(std::string str);

    //handle all lookup Callbacks from Chord Layer
    void HandleChordLookup(Ipv4Address destAddress, uint32_t transactionID);
    //handle redistribute keys callback
    void HandleChordRedistribute(uint8_t isPredecessor, Ipv4Address addrIP, uint32_t predecessorKey);

    //store to our inverted table
    void storeInvertedTableEntry(std::string keyword, std::vector<std::string> docList);

    //find intersection of two vectors
    std::vector<std::string> intersection(std::vector<std::string> v1, std::vector<std::string> v2);

    //find everything matching a list of key words 
    //modifies keywords list by removing the things we've found
    std::vector<std::string> findDocs(std::vector<std::string> &keywords);

    //Handle Publishing
    void StartPublish(std::string filename);
    void ProcessPublish(std::string keyword, std::vector<std::string> docList);
    void SendPublishReq(Ipv4Address destAddress, std::string keyword, std::vector<std::string> docList);
    void ProcessPublishReq(GUSearchMessage message, Ipv4Address sourceAddress, uint16_t sourcePort);
    
    //handle search Requests
    //
    //start a new search, wraps ProcessSearch
    void StartSearch(uint32_t nodeNumber, std::vector<std::string> keywords);
    //handle a search
    void ProcessSearch(uint32_t nodeNumber, uint32_t originatorAddr, uint32_t originatorTransactionID, 
        std::vector<std::string> keywords, std::vector<std::string> docs);
    //sends the actual searchReq packet
    void SendSearchReq(Ipv4Address destAddress, uint32_t originatorAddr, uint32_t intermediateAddr, 
        std::vector<std::string> keywords, std::vector<std::string> docList, uint32_t originatorTransactionID);
    //handle a searchRequest, wraps Process Search
    void ProcessSearchReq(GUSearchMessage message, Ipv4Address sourceAddress, uint16_t sourcePort);

    //handle search responses
    //
    void SendSearchRsp(uint32_t destAddress, uint32_t originatorTransactionID, 
        std::vector<std::string> docList);
    void ProcessSearchRsp(GUSearchMessage message, Ipv4Address sourceAddress, uint16_t sourcePort);
    //function to print a search's results
    void PrintSearchResults(uint32_t originatorTransactionID, std::vector<std::string> docList);
    
    struct PublishEvent {
      uint32_t hashedKey;
      //message payload
      std::string keyword; //keys assosciated with this req

      std::vector<std::string> docList; //sha1 hash of the key associated with it
    };

    struct SearchEvent {
      //message payload
      uint32_t originatorTransactionID;
      uint32_t originatorAddr;
      std::vector<std::string> keywords;
      std::vector<std::string> originalkeys;
      std::vector<std::string> localDocs; //if we have docs locally store them here
      uint32_t intermediateNode;
    };

  protected:
    virtual void DoDispose ();
    
  private:
    virtual void StartApplication (void);
    virtual void StopApplication (void);

    //store <key, documents>
    std::map<std::string, std::vector<std::string> > m_invertedTable;

    //map and store our PublishReqs
    std::map<uint32_t, PublishEvent> m_pendingPublish;

    //map and store our searchReqs 
    std::map<uint32_t, SearchEvent> m_pendingSearch;

    Ptr<GUChord> m_chord;
    uint32_t m_currentTransactionId;
    Ptr<Socket> m_socket;
    Time m_pingTimeout;
    uint16_t m_appPort, m_chordPort;
    // Timers
    Timer m_auditPingsTimer;
    // Ping tracker
    std::map<uint32_t, Ptr<PingRequest> > m_pingTracker;
};

#endif