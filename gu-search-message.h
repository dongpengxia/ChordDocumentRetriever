#ifndef GU_SEARCH_MESSAGE_H
#define GU_SEARCH_MESSAGE_H

#include "ns3/header.h"
#include "ns3/ipv4-address.h"
#include "ns3/packet.h"
#include "ns3/object.h"

using namespace ns3;

#define IPV4_ADDRESS_SIZE 4

class GUSearchMessage : public Header
{
  public:
    GUSearchMessage ();
    virtual ~GUSearchMessage ();


    enum MessageType
      {
        PING_REQ = 1,
        PING_RSP = 2,
        PUBLISH_REQ = 3,
        SEARCH_REQ = 4, //use this for search fwd by adding originator addr to payload
        SEARCH_RSP = 5,
      };

    GUSearchMessage (GUSearchMessage::MessageType messageType, uint32_t transactionId);

    /**
    *  \brief Sets message type
    *  \param messageType message type
    */
    void SetMessageType (MessageType messageType);

    /**
     *  \returns message type
     */
    MessageType GetMessageType () const;

    /**
     *  \brief Sets Transaction Id
     *  \param transactionId Transaction Id of the request
     */
    void SetTransactionId (uint32_t transactionId);

    /**
     *  \returns Transaction Id
     */
    uint32_t GetTransactionId () const;

  private:
    /**
     *  \cond
     */
    MessageType m_messageType;
    uint32_t m_transactionId;
    /**
     *  \endcond
     */
  public:
    static TypeId GetTypeId (void);
    virtual TypeId GetInstanceTypeId (void) const;
    void Print (std::ostream &os) const;
    uint32_t GetSerializedSize (void) const;
    void Serialize (Buffer::Iterator start) const;
    uint32_t Deserialize (Buffer::Iterator start);

    
    struct PingReq
      {
        void Print (std::ostream &os) const;
        uint32_t GetSerializedSize (void) const;
        void Serialize (Buffer::Iterator &start) const;
        uint32_t Deserialize (Buffer::Iterator &start);
        // Payload
        std::string pingMessage;
      };

    struct PingRsp
      {
        void Print (std::ostream &os) const;
        uint32_t GetSerializedSize (void) const;
        void Serialize (Buffer::Iterator &start) const;
        uint32_t Deserialize (Buffer::Iterator &start);
        // Payload
        std::string pingMessage;
      };

    struct PublishReq {
      void Print(std::ostream &os) const;
      uint32_t GetSerializedSize (void) const;
      void Serialize (Buffer::Iterator &start) const;
      uint32_t Deserialize (Buffer::Iterator &start);
      //Payload
      std::string keyword;
      std::vector<std::string> docList; //correspond to only this keyword
    };

    struct SearchReq {
      void Print(std::ostream &os) const;
      uint32_t GetSerializedSize (void) const;
      void Serialize (Buffer::Iterator &start) const;
      uint32_t Deserialize (Buffer::Iterator &start);
      //Payload
      std::vector<std::string> keywords;
      std::vector<std::string> docList;
      uint32_t originatorAddr;
      uint32_t destinationAddress;
      uint32_t originatorTransactionID;
    };

    struct SearchRsp {
      void Print(std::ostream &os) const;
      uint32_t GetSerializedSize (void) const;
      void Serialize (Buffer::Iterator &start) const;
      uint32_t Deserialize (Buffer::Iterator &start);
      //Payload
      //maps key to docs
      uint32_t originatorTransactionID;
      std::vector<std::string> docList; //corresponds to all results
    };
	
  private:
    struct
      {
        PingReq pingReq;
        PingRsp pingRsp;
        PublishReq publishReq;
        SearchReq searchReq;
        SearchRsp searchRsp;
      } m_message;
    
  public:
    /**
     *  \returns PingReq Struct
     */
    PingReq GetPingReq ();

    /**
     *  \brief Sets PingReq message params
     *  \param message Payload String
     */

    void SetPingReq (std::string message);

    /**
     * \returns PingRsp Struct
     */
    PingRsp GetPingRsp ();
    /**
     *  \brief Sets PingRsp message params
     *  \param message Payload String
     */
    void SetPingRsp (std::string message);

    PublishReq getPublishReq();
    void setPublishReq(std::string keyword, std::vector<std::string> docList);

    SearchReq getSearchReq();
    void setSearchReq(std::vector<std::string> keywords, std::vector<std::string> docList, 
    uint32_t originatorAddr, uint32_t destinationAddress, uint32_t originatorTransactionID);

    SearchRsp getSearchRsp();
    void setSearchRsp(std::vector<std::string> docList, uint32_t originatorTransactionID);

};

static inline std::ostream& operator<< (std::ostream& os, const GUSearchMessage& message)
{
  message.Print (os);
  return os;
}

#endif