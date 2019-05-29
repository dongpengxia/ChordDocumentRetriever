#ifndef GU_CHORD_MESSAGE_H
#define GU_CHORD_MESSAGE_H

#include "ns3/header.h"
#include "ns3/ipv4-address.h"
#include "ns3/packet.h"
#include "ns3/object.h"

using namespace ns3;

#define IPV4_ADDRESS_SIZE 4

class GUChordMessage : public Header
{
  public:
    GUChordMessage ();
    virtual ~GUChordMessage ();
    
    enum MessageType
      {
        PING_REQ = 1,
        PING_RSP = 2,
        SUCCESSOR_REQ = 3,
        SUCCESSOR_RSP = 4,
        NOTIFY_SUCCESSOR = 5,
        NOTIFY_PREDECESSOR = 6,
        GET_PREDECESSOR_OF_SUCCESSOR_REQ = 7,
        GET_PREDECESSOR_OF_SUCCESSOR_RSP = 8,
        RINGSTATE_REQ = 9,
        LOOKUP_REQ = 10,
        LOOKUP_RSP = 11,
        NODELEAVE = 12,
      };
    
    GUChordMessage (GUChordMessage::MessageType messageType, uint32_t transactionId);
    
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

    struct SuccessorReq 
    	{
        void Print(std::ostream &os) const;
        uint32_t GetSerializedSize(void) const;
        void Serialize(Buffer::Iterator &start) const;
        uint32_t Deserialize (Buffer::Iterator &start);
        //PayLoad
        uint32_t originalNodeNum; //original requestor's node number
        uint32_t chordKey; //original requestor's chord index, so we can quickly test if it's between current node and successor
      };

    struct SuccessorRsp
    	{
        void Print(std::ostream &os) const;
        uint32_t GetSerializedSize(void) const;
        void Serialize(Buffer::Iterator &start) const;
        uint32_t Deserialize (Buffer::Iterator &start);
        //PayLoad
        uint32_t successorNodeNum; //successor node number for original requestor of SuccessorReq
        uint32_t successorChordKey;//successor index for original requestor of SuccessorReq
      };

    struct PredecessorOfSuccessorRsp
      {
      	void Print(std::ostream &os) const;
	      uint32_t GetSerializedSize(void) const;
	      void Serialize(Buffer::Iterator &start) const;
	      uint32_t Deserialize (Buffer::Iterator &start);
	      //PayLoad
	      uint32_t predecessorNodeNum; //sending node's predecessor's node number
	      uint32_t predecessorChordKey;//sending node's predecessor's index
      };

    struct RingstateReq 
    	{
        void Print(std::ostream &os) const;
        uint32_t GetSerializedSize(void) const;
        void Serialize(Buffer::Iterator &start) const;
        uint32_t Deserialize (Buffer::Iterator &start);
        //PayLoad
        uint32_t originalNodeNum; //original requestor's node number
      };

      struct LookupReq 
      {
        void Print(std::ostream &os) const;
        uint32_t GetSerializedSize(void) const;
        void Serialize(Buffer::Iterator &start) const;
        uint32_t Deserialize (Buffer::Iterator &start);
        //PayLoad
        uint32_t originalNodeNum; //original requestor's node number
        uint32_t lookupKey;       //hash key we want to find the successor for
        uint8_t fromChord;        //1 if originally initiated from chord, 0 otherwise
      };

      struct LookupRsp
      {
        void Print(std::ostream &os) const;
        uint32_t GetSerializedSize(void) const;
        void Serialize(Buffer::Iterator &start) const;
        uint32_t Deserialize (Buffer::Iterator &start);
        //PayLoad
        uint32_t successorNodeNum; //successor to lookedup key
        uint32_t lookupKey;       //hash key we were finding successor for
        uint8_t fromChord;
      };

      struct NodeLeave {
        void Print(std::ostream &os) const;
        uint32_t GetSerializedSize(void) const;
        void Serialize(Buffer::Iterator &start) const;
        uint32_t Deserialize (Buffer::Iterator &start);
        //PayLoad
        //this allows us to use one packet type for two cases
        uint8_t isSuccessor; //treat as a boolean
        uint32_t hashedNode; //if isSuccessorIP != 0, new successor's hashed key = this
        uint32_t ipAddr; //if isSuccessorIP != 0, new successor = ipAddr
      };

  private:
    struct
      {
        PingReq pingReq;
        PingRsp pingRsp;

        SuccessorReq successorReq;
        SuccessorRsp successorRsp;
        PredecessorOfSuccessorRsp predecessorOfSuccessorRsp;
        RingstateReq ringstateReq;
	      
        LookupReq lookupReq;
        LookupRsp lookupRsp;
	      
        NodeLeave nodeLeave;

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

    //setters and getters
    SuccessorReq GetSuccessorReq();
    void SetSuccessorReq(uint32_t originalNodeNum, uint32_t chordKey);
    SuccessorRsp GetSuccessorRsp();
    void SetSuccessorRsp(uint32_t successorNodeNum, uint32_t successorChordKey);
    PredecessorOfSuccessorRsp GetPredecessorOfSuccessorRsp();
    void SetPredecessorOfSuccessorRsp(uint32_t predecessorNodeNum, uint32_t predecessorChordKey);
    RingstateReq GetRingstateReq();
    void SetRingstateReq(uint32_t originalNodeNum);
	
    LookupReq GetLookupReq();
    void SetLookupReq(uint32_t originalNodeNum, uint32_t lookupKey, uint8_t fromChord);

    LookupRsp GetLookupRsp();
    void SetLookupRsp(uint32_t successorNodeNum, uint32_t lookupKey, uint8_t fromChord);

    NodeLeave GetNodeLeave();
    void SetNodeLeave(uint8_t isSuccessor, uint32_t hashedNode, uint32_t ipAddr);
};

static inline std::ostream& operator<< (std::ostream& os, const GUChordMessage& message)
{
  message.Print (os);
  return os;
}

#endif