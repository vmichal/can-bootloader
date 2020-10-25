#include "can_Bootloader.h"
#include <string.h>

CAN_msg_status_t Bootloader_Beacon_status;
Bootloader_Beacon_t Bootloader_Beacon_data;
int32_t Bootloader_Beacon_last_sent;
CAN_msg_status_t Bootloader_Data_status;
Bootloader_Data_t Bootloader_Data_data;
CAN_msg_status_t Bootloader_DataAck_status;
Bootloader_DataAck_t Bootloader_DataAck_data;
CAN_msg_status_t Bootloader_ExitReq_status;
Bootloader_ExitReq_t Bootloader_ExitReq_data;
CAN_msg_status_t Bootloader_Handshake_status;
Bootloader_Handshake_t Bootloader_Handshake_data;
CAN_msg_status_t Bootloader_HandshakeAck_status;
Bootloader_HandshakeAck_t Bootloader_HandshakeAck_data;
int32_t Bootloader_SoftwareBuild_last_sent;
CAN_msg_status_t Bootloader_CommunicationYield_status;
Bootloader_CommunicationYield_t Bootloader_CommunicationYield_data;

void candbInit(void) {
    canInitMsgStatus(&Bootloader_Beacon_status, 1000);
    Bootloader_Beacon_last_sent = -1;
    canInitMsgStatus(&Bootloader_Data_status, -1);
    canInitMsgStatus(&Bootloader_DataAck_status, -1);
    canInitMsgStatus(&Bootloader_ExitReq_status, -1);
    canInitMsgStatus(&Bootloader_Handshake_status, -1);
    canInitMsgStatus(&Bootloader_HandshakeAck_status, -1);
    Bootloader_SoftwareBuild_last_sent = -1;
    canInitMsgStatus(&Bootloader_CommunicationYield_status, -1);
}

int Bootloader_decode_Beacon_s(const uint8_t* bytes, size_t length, Bootloader_Beacon_t* data_out) {
    if (length < 3)
        return 0;

    data_out->Target = (enum Bootloader_BootTarget) ((bytes[0] & 0x0F));
    data_out->State = (enum Bootloader_State) (((bytes[0] >> 4) & 0x0F));
    data_out->EntryReason = (enum Bootloader_EntryReason) ((bytes[1] & 0x0F));
    data_out->FlashSize = ((bytes[1] >> 4) & 0x0F) | bytes[2] << 4;
    return 1;
}

int Bootloader_decode_Beacon(const uint8_t* bytes, size_t length, enum Bootloader_BootTarget* Target_out, enum Bootloader_State* State_out, enum Bootloader_EntryReason* EntryReason_out, uint16_t* FlashSize_out) {
    if (length < 3)
        return 0;

    *Target_out = (enum Bootloader_BootTarget) ((bytes[0] & 0x0F));
    *State_out = (enum Bootloader_State) (((bytes[0] >> 4) & 0x0F));
    *EntryReason_out = (enum Bootloader_EntryReason) ((bytes[1] & 0x0F));
    *FlashSize_out = ((bytes[1] >> 4) & 0x0F) | bytes[2] << 4;
    return 1;
}

int Bootloader_get_Beacon(Bootloader_Beacon_t* data_out) {
    if (!(Bootloader_Beacon_status.flags & CAN_MSG_RECEIVED))
        return 0;

#ifndef CANDB_IGNORE_TIMEOUTS
    if (txGetTimeMillis() > (uint32_t)(Bootloader_Beacon_status.timestamp + Bootloader_Beacon_timeout))
        return 0;
#endif

    if (data_out)
        memcpy(data_out, &Bootloader_Beacon_data, sizeof(Bootloader_Beacon_t));

    int flags = Bootloader_Beacon_status.flags;
    Bootloader_Beacon_status.flags &= ~CAN_MSG_PENDING;
    return flags;
}

int Bootloader_send_Beacon_s(const Bootloader_Beacon_t* data) {
    uint8_t buffer[3];
    buffer[0] = (data->Target & 0x0F) | ((data->State & 0x0F) << 4);
    buffer[1] = (data->EntryReason & 0x0F) | ((data->FlashSize & 0x0F) << 4);
    buffer[2] = (data->FlashSize >> 4);
    int rc = txSendCANMessage(bus_CAN1, Bootloader_Beacon_id, buffer, sizeof(buffer))
        && txSendCANMessage(bus_CAN2, Bootloader_Beacon_id, buffer, sizeof(buffer));

    if (rc == 0) {
        Bootloader_Beacon_last_sent = txGetTimeMillis();
    }

    return rc;
}

int Bootloader_send_Beacon(enum Bootloader_BootTarget Target, enum Bootloader_State State, enum Bootloader_EntryReason EntryReason, uint16_t FlashSize) {
    uint8_t buffer[3];
    buffer[0] = (Target & 0x0F) | ((State & 0x0F) << 4);
    buffer[1] = (EntryReason & 0x0F) | ((FlashSize & 0x0F) << 4);
    buffer[2] = (FlashSize >> 4);
    int rc = txSendCANMessage(bus_CAN1, Bootloader_Beacon_id, buffer, sizeof(buffer))
        && txSendCANMessage(bus_CAN2, Bootloader_Beacon_id, buffer, sizeof(buffer));

    if (rc == 0) {
        Bootloader_Beacon_last_sent = txGetTimeMillis();
    }

    return rc;
}

int Bootloader_Beacon_need_to_send(void) {
    return (Bootloader_Beacon_last_sent == -1) || (txGetTimeMillis() >= (uint32_t)(Bootloader_Beacon_last_sent + 500));
}

void Bootloader_Beacon_on_receive(int (*callback)(Bootloader_Beacon_t* data)) {
    Bootloader_Beacon_status.on_receive = (void (*)(void)) callback;
}

int Bootloader_decode_Data_s(const uint8_t* bytes, size_t length, Bootloader_Data_t* data_out) {
    if (length < 8)
        return 0;

    data_out->Address = bytes[0] | bytes[1] << 8 | bytes[2] << 16 | (bytes[3] & 0x3F) << 24;
    data_out->HalfwordAccess = ((bytes[3] >> 6) & 0x01);
    data_out->Word = bytes[4] | bytes[5] << 8 | bytes[6] << 16 | bytes[7] << 24;
    return 1;
}

int Bootloader_decode_Data(const uint8_t* bytes, size_t length, uint32_t* Address_out, uint8_t* HalfwordAccess_out, uint32_t* Word_out) {
    if (length < 8)
        return 0;

    *Address_out = bytes[0] | bytes[1] << 8 | bytes[2] << 16 | (bytes[3] & 0x3F) << 24;
    *HalfwordAccess_out = ((bytes[3] >> 6) & 0x01);
    *Word_out = bytes[4] | bytes[5] << 8 | bytes[6] << 16 | bytes[7] << 24;
    return 1;
}

int Bootloader_get_Data(Bootloader_Data_t* data_out) {
    if (!(Bootloader_Data_status.flags & CAN_MSG_RECEIVED))
        return 0;

    if (data_out)
        memcpy(data_out, &Bootloader_Data_data, sizeof(Bootloader_Data_t));

    int flags = Bootloader_Data_status.flags;
    Bootloader_Data_status.flags &= ~CAN_MSG_PENDING;
    return flags;
}

int Bootloader_send_Data_s(const Bootloader_Data_t* data) {
    uint8_t buffer[8];
    buffer[0] = data->Address;
    buffer[1] = (data->Address >> 8);
    buffer[2] = (data->Address >> 16);
    buffer[3] = ((data->Address >> 24) & 0x3F) | (data->HalfwordAccess ? 64 : 0);
    buffer[4] = data->Word;
    buffer[5] = (data->Word >> 8);
    buffer[6] = (data->Word >> 16);
    buffer[7] = (data->Word >> 24);
    int rc = txSendCANMessage(Bootloader_Handshake_status.bus, Bootloader_Data_id, buffer, sizeof(buffer));
    return rc;
}

int Bootloader_send_Data(uint32_t Address, uint8_t HalfwordAccess, uint32_t Word) {
    uint8_t buffer[8];
    buffer[0] = Address;
    buffer[1] = (Address >> 8);
    buffer[2] = (Address >> 16);
    buffer[3] = ((Address >> 24) & 0x3F) | (HalfwordAccess ? 64 : 0);
    buffer[4] = Word;
    buffer[5] = (Word >> 8);
    buffer[6] = (Word >> 16);
    buffer[7] = (Word >> 24);
    int rc = txSendCANMessage(Bootloader_Handshake_status.bus, Bootloader_Data_id, buffer, sizeof(buffer));
    return rc;
}

void Bootloader_Data_on_receive(int (*callback)(Bootloader_Data_t* data)) {
    Bootloader_Data_status.on_receive = (void (*)(void)) callback;
}

int Bootloader_decode_DataAck_s(const uint8_t* bytes, size_t length, Bootloader_DataAck_t* data_out) {
    if (length < 4)
        return 0;

    data_out->Address = bytes[0] | bytes[1] << 8 | bytes[2] << 16 | (bytes[3] & 0x3F) << 24;
    data_out->Result = (enum Bootloader_WriteResult) (((bytes[3] >> 6) & 0x03));
    return 1;
}

int Bootloader_decode_DataAck(const uint8_t* bytes, size_t length, uint32_t* Address_out, enum Bootloader_WriteResult* Result_out) {
    if (length < 4)
        return 0;

    *Address_out = bytes[0] | bytes[1] << 8 | bytes[2] << 16 | (bytes[3] & 0x3F) << 24;
    *Result_out = (enum Bootloader_WriteResult) (((bytes[3] >> 6) & 0x03));
    return 1;
}

int Bootloader_get_DataAck(Bootloader_DataAck_t* data_out) {
    if (!(Bootloader_DataAck_status.flags & CAN_MSG_RECEIVED))
        return 0;

    if (data_out)
        memcpy(data_out, &Bootloader_DataAck_data, sizeof(Bootloader_DataAck_t));

    int flags = Bootloader_DataAck_status.flags;
    Bootloader_DataAck_status.flags &= ~CAN_MSG_PENDING;
    return flags;
}

int Bootloader_send_DataAck_s(const Bootloader_DataAck_t* data) {
    uint8_t buffer[4];
    buffer[0] = data->Address;
    buffer[1] = (data->Address >> 8);
    buffer[2] = (data->Address >> 16);
    buffer[3] = ((data->Address >> 24) & 0x3F) | ((data->Result & 0x03) << 6);
    int rc = txSendCANMessage(Bootloader_Handshake_status.bus, Bootloader_DataAck_id, buffer, sizeof(buffer));
    return rc;
}

int Bootloader_send_DataAck(uint32_t Address, enum Bootloader_WriteResult Result) {
    uint8_t buffer[4];
    buffer[0] = Address;
    buffer[1] = (Address >> 8);
    buffer[2] = (Address >> 16);
    buffer[3] = ((Address >> 24) & 0x3F) | ((Result & 0x03) << 6);
    int rc = txSendCANMessage(Bootloader_Handshake_status.bus, Bootloader_DataAck_id, buffer, sizeof(buffer));
    return rc;
}

void Bootloader_DataAck_on_receive(int (*callback)(Bootloader_DataAck_t* data)) {
    Bootloader_DataAck_status.on_receive = (void (*)(void)) callback;
}

int Bootloader_decode_ExitReq_s(const uint8_t* bytes, size_t length, Bootloader_ExitReq_t* data_out) {
    if (length < 1)
        return 0;

    data_out->Target = (enum Bootloader_BootTarget) ((bytes[0] & 0x0F));
    data_out->Force = ((bytes[0] >> 4) & 0x01);
    return 1;
}

int Bootloader_decode_ExitReq(const uint8_t* bytes, size_t length, enum Bootloader_BootTarget* Target_out, uint8_t* Force_out) {
    if (length < 1)
        return 0;

    *Target_out = (enum Bootloader_BootTarget) ((bytes[0] & 0x0F));
    *Force_out = ((bytes[0] >> 4) & 0x01);
    return 1;
}

int Bootloader_get_ExitReq(Bootloader_ExitReq_t* data_out) {
    if (!(Bootloader_ExitReq_status.flags & CAN_MSG_RECEIVED))
        return 0;

    if (data_out)
        memcpy(data_out, &Bootloader_ExitReq_data, sizeof(Bootloader_ExitReq_t));

    int flags = Bootloader_ExitReq_status.flags;
    Bootloader_ExitReq_status.flags &= ~CAN_MSG_PENDING;
    return flags;
}

void Bootloader_ExitReq_on_receive(int (*callback)(Bootloader_ExitReq_t* data)) {
    Bootloader_ExitReq_status.on_receive = (void (*)(void)) callback;
}

int Bootloader_send_ExitAck_s(const Bootloader_ExitAck_t* data) {
    uint8_t buffer[1];
    buffer[0] = (data->Target & 0x0F) | (data->Confirmed ? 16 : 0);
    int rc = txSendCANMessage(Bootloader_ExitReq_status.bus, Bootloader_ExitAck_id, buffer, sizeof(buffer));
    return rc;
}

int Bootloader_send_ExitAck(enum Bootloader_BootTarget Target, uint8_t Confirmed) {
    uint8_t buffer[1];
    buffer[0] = (Target & 0x0F) | (Confirmed ? 16 : 0);
    int rc = txSendCANMessage(Bootloader_ExitReq_status.bus, Bootloader_ExitAck_id, buffer, sizeof(buffer));
    return rc;
}

int Bootloader_decode_Handshake_s(const uint8_t* bytes, size_t length, Bootloader_Handshake_t* data_out) {
    if (length < 5)
        return 0;

    data_out->Register = (enum Bootloader_Register) ((bytes[0] & 0x0F));
    data_out->Command = (enum Bootloader_Command) (((bytes[0] >> 4) & 0x0F));
    data_out->Value = bytes[1] | bytes[2] << 8 | bytes[3] << 16 | bytes[4] << 24;
    return 1;
}

int Bootloader_decode_Handshake(const uint8_t* bytes, size_t length, enum Bootloader_Register* Register_out, enum Bootloader_Command* Command_out, uint32_t* Value_out) {
    if (length < 5)
        return 0;

    *Register_out = (enum Bootloader_Register) ((bytes[0] & 0x0F));
    *Command_out = (enum Bootloader_Command) (((bytes[0] >> 4) & 0x0F));
    *Value_out = bytes[1] | bytes[2] << 8 | bytes[3] << 16 | bytes[4] << 24;
    return 1;
}

int Bootloader_get_Handshake(Bootloader_Handshake_t* data_out) {
    if (!(Bootloader_Handshake_status.flags & CAN_MSG_RECEIVED))
        return 0;

    if (data_out)
        memcpy(data_out, &Bootloader_Handshake_data, sizeof(Bootloader_Handshake_t));

    int flags = Bootloader_Handshake_status.flags;
    Bootloader_Handshake_status.flags &= ~CAN_MSG_PENDING;
    return flags;
}

int Bootloader_send_Handshake_s(const Bootloader_Handshake_t* data) {
    uint8_t buffer[5];
    buffer[0] = (data->Register & 0x0F) | ((data->Command & 0x0F) << 4);
    buffer[1] = data->Value;
    buffer[2] = (data->Value >> 8);
    buffer[3] = (data->Value >> 16);
    buffer[4] = (data->Value >> 24);
    int rc = txSendCANMessage(Bootloader_Handshake_status.bus, Bootloader_Handshake_id, buffer, sizeof(buffer));
    return rc;
}

int Bootloader_send_Handshake(enum Bootloader_Register Register, enum Bootloader_Command Command, uint32_t Value) {
    uint8_t buffer[5];
    buffer[0] = (Register & 0x0F) | ((Command & 0x0F) << 4);
    buffer[1] = Value;
    buffer[2] = (Value >> 8);
    buffer[3] = (Value >> 16);
    buffer[4] = (Value >> 24);
    int rc = txSendCANMessage(Bootloader_Handshake_status.bus, Bootloader_Handshake_id, buffer, sizeof(buffer));
    return rc;
}

void Bootloader_Handshake_on_receive(int (*callback)(Bootloader_Handshake_t* data)) {
    Bootloader_Handshake_status.on_receive = (void (*)(void)) callback;
}

int Bootloader_decode_HandshakeAck_s(const uint8_t* bytes, size_t length, Bootloader_HandshakeAck_t* data_out) {
    if (length < 6)
        return 0;

    data_out->Register = (enum Bootloader_Register) ((bytes[0] & 0x0F));
    data_out->Response = (enum Bootloader_HandshakeResponse) ((bytes[1] & 0x1F));
    data_out->Value = bytes[2] | bytes[3] << 8 | bytes[4] << 16 | bytes[5] << 24;
    return 1;
}

int Bootloader_decode_HandshakeAck(const uint8_t* bytes, size_t length, enum Bootloader_Register* Register_out, enum Bootloader_HandshakeResponse* Response_out, uint32_t* Value_out) {
    if (length < 6)
        return 0;

    *Register_out = (enum Bootloader_Register) ((bytes[0] & 0x0F));
    *Response_out = (enum Bootloader_HandshakeResponse) ((bytes[1] & 0x1F));
    *Value_out = bytes[2] | bytes[3] << 8 | bytes[4] << 16 | bytes[5] << 24;
    return 1;
}

int Bootloader_get_HandshakeAck(Bootloader_HandshakeAck_t* data_out) {
    if (!(Bootloader_HandshakeAck_status.flags & CAN_MSG_RECEIVED))
        return 0;

    if (data_out)
        memcpy(data_out, &Bootloader_HandshakeAck_data, sizeof(Bootloader_HandshakeAck_t));

    int flags = Bootloader_HandshakeAck_status.flags;
    Bootloader_HandshakeAck_status.flags &= ~CAN_MSG_PENDING;
    return flags;
}

int Bootloader_send_HandshakeAck_s(const Bootloader_HandshakeAck_t* data) {
    uint8_t buffer[6];
    buffer[0] = (data->Register & 0x0F);
    buffer[1] = (data->Response & 0x1F);
    buffer[2] = data->Value;
    buffer[3] = (data->Value >> 8);
    buffer[4] = (data->Value >> 16);
    buffer[5] = (data->Value >> 24);
    int rc = txSendCANMessage(Bootloader_Handshake_status.bus, Bootloader_HandshakeAck_id, buffer, sizeof(buffer));
    return rc;
}

int Bootloader_send_HandshakeAck(enum Bootloader_Register Register, enum Bootloader_HandshakeResponse Response, uint32_t Value) {
    uint8_t buffer[6];
    buffer[0] = (Register & 0x0F);
    buffer[1] = (Response & 0x1F);
    buffer[2] = Value;
    buffer[3] = (Value >> 8);
    buffer[4] = (Value >> 16);
    buffer[5] = (Value >> 24);
    int rc = txSendCANMessage(Bootloader_Handshake_status.bus, Bootloader_HandshakeAck_id, buffer, sizeof(buffer));
    return rc;
}

void Bootloader_HandshakeAck_on_receive(int (*callback)(Bootloader_HandshakeAck_t* data)) {
    Bootloader_HandshakeAck_status.on_receive = (void (*)(void)) callback;
}

int Bootloader_send_SoftwareBuild_s(const Bootloader_SoftwareBuild_t* data) {
    uint8_t buffer[5];
    buffer[0] = data->CommitSHA;
    buffer[1] = (data->CommitSHA >> 8);
    buffer[2] = (data->CommitSHA >> 16);
    buffer[3] = (data->CommitSHA >> 24);
    buffer[4] = (data->DirtyRepo ? 1 : 0) | ((data->Target & 0x0F) << 4);
    int rc = txSendCANMessage(bus_CAN1, Bootloader_SoftwareBuild_id, buffer, sizeof(buffer))
        && txSendCANMessage(bus_CAN2, Bootloader_SoftwareBuild_id, buffer, sizeof(buffer));

    if (rc == 0) {
        Bootloader_SoftwareBuild_last_sent = txGetTimeMillis();
    }

    return rc;
}

int Bootloader_send_SoftwareBuild(uint32_t CommitSHA, uint8_t DirtyRepo, enum Bootloader_BootTarget Target) {
    uint8_t buffer[5];
    buffer[0] = CommitSHA;
    buffer[1] = (CommitSHA >> 8);
    buffer[2] = (CommitSHA >> 16);
    buffer[3] = (CommitSHA >> 24);
    buffer[4] = (DirtyRepo ? 1 : 0) | ((Target & 0x0F) << 4);
    int rc = txSendCANMessage(bus_CAN1, Bootloader_SoftwareBuild_id, buffer, sizeof(buffer))
        && txSendCANMessage(bus_CAN2, Bootloader_SoftwareBuild_id, buffer, sizeof(buffer));

    if (rc == 0) {
        Bootloader_SoftwareBuild_last_sent = txGetTimeMillis();
    }

    return rc;
}

int Bootloader_SoftwareBuild_need_to_send(void) {
    return (Bootloader_SoftwareBuild_last_sent == -1) || (txGetTimeMillis() >= (uint32_t)(Bootloader_SoftwareBuild_last_sent + 10000));
}

int Bootloader_decode_CommunicationYield_s(const uint8_t* bytes, size_t length, Bootloader_CommunicationYield_t* data_out) {
    if (length < 1)
        return 0;

    data_out->Target = (enum Bootloader_BootTarget)((bytes[0] & 0x0F));
    return 1;
}

int Bootloader_decode_CommunicationYield(const uint8_t* bytes, size_t length, enum Bootloader_BootTarget* Target_out) {
    if (length < 1)
        return 0;

    *Target_out = (enum Bootloader_BootTarget)((bytes[0] & 0x0F));
    return 1;
}

int Bootloader_get_CommunicationYield(Bootloader_CommunicationYield_t* data_out) {
    if (!(Bootloader_CommunicationYield_status.flags & CAN_MSG_RECEIVED))
        return 0;

    if (data_out)
        memcpy(data_out, &Bootloader_CommunicationYield_data, sizeof(Bootloader_CommunicationYield_t));

    int flags = Bootloader_CommunicationYield_status.flags;
    Bootloader_CommunicationYield_status.flags &= ~CAN_MSG_PENDING;
    return flags;
}

int Bootloader_send_CommunicationYield_s(const Bootloader_CommunicationYield_t* data) {
    uint8_t buffer[1];
    buffer[0] = (data->Target & 0x0F);
    int rc = txSendCANMessage(Bootloader_CommunicationYield_status.bus, Bootloader_CommunicationYield_id, buffer, sizeof(buffer));
    return rc;
}

int Bootloader_send_CommunicationYield(enum Bootloader_BootTarget Target) {
    uint8_t buffer[1];
    buffer[0] = (Target & 0x0F);
    int rc = txSendCANMessage(Bootloader_CommunicationYield_status.bus, Bootloader_CommunicationYield_id, buffer, sizeof(buffer));
    return rc;
}

void Bootloader_CommunicationYield_on_receive(int (*callback)(Bootloader_CommunicationYield_t* data)) {
    Bootloader_CommunicationYield_status.on_receive = (void (*)(void)) callback;
}

void candbHandleMessage(uint32_t timestamp, int bus, CAN_ID_t id, const uint8_t* payload, size_t payload_length) {
    switch (id) {
    case Bootloader_Beacon_id: {
        if (!Bootloader_decode_Beacon_s(payload, payload_length, &Bootloader_Beacon_data))
            break;

        canUpdateMsgStatusOnReceive(&Bootloader_Beacon_status, timestamp, bus);

        if (Bootloader_Beacon_status.on_receive)
            ((int (*)(Bootloader_Beacon_t*)) Bootloader_Beacon_status.on_receive)(&Bootloader_Beacon_data);

        break;
    }
    case Bootloader_Data_id: {
        if (!Bootloader_decode_Data_s(payload, payload_length, &Bootloader_Data_data))
            break;

        canUpdateMsgStatusOnReceive(&Bootloader_Data_status, timestamp, bus);

        if (Bootloader_Data_status.on_receive)
            ((int (*)(Bootloader_Data_t*)) Bootloader_Data_status.on_receive)(&Bootloader_Data_data);

        break;
    }
    case Bootloader_DataAck_id: {
        if (!Bootloader_decode_DataAck_s(payload, payload_length, &Bootloader_DataAck_data))
            break;

        canUpdateMsgStatusOnReceive(&Bootloader_DataAck_status, timestamp, bus);

        if (Bootloader_DataAck_status.on_receive)
            ((int (*)(Bootloader_DataAck_t*)) Bootloader_DataAck_status.on_receive)(&Bootloader_DataAck_data);

        break;
    }
    case Bootloader_ExitReq_id: {
        if (!Bootloader_decode_ExitReq_s(payload, payload_length, &Bootloader_ExitReq_data))
            break;

        canUpdateMsgStatusOnReceive(&Bootloader_ExitReq_status, timestamp, bus);

        if (Bootloader_ExitReq_status.on_receive)
            ((int (*)(Bootloader_ExitReq_t*)) Bootloader_ExitReq_status.on_receive)(&Bootloader_ExitReq_data);

        break;
    }
    case Bootloader_Handshake_id: {
        if (!Bootloader_decode_Handshake_s(payload, payload_length, &Bootloader_Handshake_data))
            break;

        canUpdateMsgStatusOnReceive(&Bootloader_Handshake_status, timestamp, bus);

        if (Bootloader_Handshake_status.on_receive)
            ((int (*)(Bootloader_Handshake_t*)) Bootloader_Handshake_status.on_receive)(&Bootloader_Handshake_data);

        break;
    }
    case Bootloader_HandshakeAck_id: {
        if (!Bootloader_decode_HandshakeAck_s(payload, payload_length, &Bootloader_HandshakeAck_data))
            break;

        canUpdateMsgStatusOnReceive(&Bootloader_HandshakeAck_status, timestamp, bus);

        if (Bootloader_HandshakeAck_status.on_receive)
            ((int (*)(Bootloader_HandshakeAck_t*)) Bootloader_HandshakeAck_status.on_receive)(&Bootloader_HandshakeAck_data);

        break;
    }
    case Bootloader_CommunicationYield_id: {
        if (!Bootloader_decode_CommunicationYield_s(payload, payload_length, &Bootloader_CommunicationYield_data))
            break;

        canUpdateMsgStatusOnReceive(&Bootloader_CommunicationYield_status, timestamp, bus);

        if (Bootloader_CommunicationYield_status.on_receive)
            ((int (*)(Bootloader_CommunicationYield_t*)) Bootloader_CommunicationYield_status.on_receive)(&Bootloader_CommunicationYield_data);

        break;
    }
    }
}
