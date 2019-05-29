#ifndef LS_MESSAGE_H
#define LS_MESSAGE_H

#include "ns3/header.h"
#include "ns3/ipv4-address.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include <set>

using namespace ns3;

#define IPV4_ADDRESS_SIZE 4

class LSMessage : public Header
{
  public:
    enum MessageType
    {
        PING_REQ = 1,
        PING_RSP = 2, 

      	//message types for neighbor discovery
      	NEIGHBOR_REQ = 3,
      	NEIGHBOR_RSP = 4,
        //message type for link state packet
        LSP = 5
    };

	LSMessage ();
    LSMessage (LSMessage::MessageType messageType, uint32_t sequenceNumber, uint8_t ttl, Ipv4Address originatorAddress);
    virtual ~LSMessage ();

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
     *  \brief Sets Sequence Number
     *  \param sequenceNumber Sequence Number of the request
     */
    void SetSequenceNumber (uint32_t sequenceNumber);

    /**
     *  \returns Sequence Number
     */
    uint32_t GetSequenceNumber () const;

    /**
     *  \brief Sets Originator IP Address
     *  \param originatorAddress Originator IPV4 address
     */
    void SetOriginatorAddress (Ipv4Address originatorAddress);

    /** 
     *  \returns Originator IPV4 address
     */
    Ipv4Address GetOriginatorAddress () const;

    /**
     *  \brief Sets Time To Live of the message 
     *  \param ttl TTL of the message
     */
    void SetTTL (uint8_t ttl);

    /** 
     *  \returns TTL of the message
     */
    uint8_t GetTTL () const;

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
        Ipv4Address destinationAddress;
        std::string pingMessage;
      };

    struct PingRsp
      {
        void Print (std::ostream &os) const;
        uint32_t GetSerializedSize (void) const;
        void Serialize (Buffer::Iterator &start) const;
        uint32_t Deserialize (Buffer::Iterator &start);
        // Payload
        Ipv4Address destinationAddress;
        std::string pingMessage;
      };

    //Structs for neighbor discovery

    //Neighbor Request
    struct NeighborReq
      {
      	void Print (std::ostream &os) const;
        uint32_t GetSerializedSize (void) const;
        void Serialize (Buffer::Iterator &start) const;
        uint32_t Deserialize (Buffer::Iterator &start);
        // Payload
        std::string neighborMessage;
      };

    //Neighbor Response
    struct NeighborRsp
      {
      	void Print (std::ostream &os) const;
        uint32_t GetSerializedSize (void) const;
        void Serialize (Buffer::Iterator &start) const;
        uint32_t Deserialize (Buffer::Iterator &start);
        // Payload
        Ipv4Address destinationAddress;
        std::string neighborMessage;
      };
	
    //Struct for Link State Packet (a list of neighbors to a node)

    struct LSPacket 
      {
        void Print(std::ostream &os) const;
        uint32_t GetSerializedSize(void) const;
        void Serialize (Buffer::Iterator &start) const;
        uint32_t Deserialize (Buffer::Iterator &start);
        // Payload
        //list of immediate neighbors to a node
        std::set<uint32_t> neighbors;
      };

    /**
     *  \returns PingReq Struct
     */
    PingReq GetPingReq ();

    /**
     *  \brief Sets PingReq message params
     *  \param message Payload String
     */
    void SetPingReq (Ipv4Address destinationAddress, std::string message);

    /**
     * \returns PingRsp Struct
     */
    PingRsp GetPingRsp ();

    /**
     *  \brief Sets PingRsp message params
     *  \param message Payload String
     */
    void SetPingRsp (Ipv4Address destinationAddress, std::string message);

    //setters and getters for neighbor discovery req and rsp
    NeighborReq GetNeighborReq ();
    void SetNeighborReq (std::string message);
	NeighborRsp GetNeighborRsp ();
	void SetNeighborRsp (Ipv4Address destinationAddress, std::string message);

    //setters and getters for link state packet
    LSPacket GetLSPacket ();
    void SetLSPacket (std::set<uint32_t> neighbors);
	
  private:
    MessageType m_messageType;
    uint32_t m_sequenceNumber;
    Ipv4Address m_originatorAddress;
    uint8_t m_ttl;

    struct
      {
        PingReq pingReq;
        PingRsp pingRsp;

        //message types for neighbor discovery
        NeighborReq neighborReq;
        NeighborRsp neighborRsp;
        //message type for LS Packet
        LSPacket lsPacket;
      } m_message;
};

static inline std::ostream& operator<< (std::ostream& os, const LSMessage& message)
{
  message.Print (os);
  return os;
}

#endif