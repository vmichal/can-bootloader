#include "can_Bootloader.h"
#include <string.h>

CAN_msg_status_t Bootloader_EntryReq_status;
Bootloader_EntryReq_t Bootloader_EntryReq_data;
int32_t Bootloader_BootloaderBeacon_last_sent;
CAN_msg_status_t Bootloader_Data_status;
Bootloader_Data_t Bootloader_Data_data;
CAN_msg_status_t Bootloader_Handshake_status;
Bootloader_Handshake_t Bootloader_Handshake_data;
int32_t Bootloader_SoftwareBuild_last_sent;
int32_t Bootloader_SerialOutput_last_sent;

void candbInit(void) {
    canInitMsgStatus(&Bootloader_EntryReq_status, -1);
    Bootloader_BootloaderBeacon_last_sent = -1;
    canInitMsgStatus(&Bootloader_Data_status, -1);
    canInitMsgStatus(&Bootloader_Handshake_status, -1);
    Bootloader_SoftwareBuild_last_sent = -1;
    Bootloader_SerialOutput_last_sent = -1;
}

int Bootloader_decode_EntryReq_s(const uint8_t* bytes, size_t length, Bootloader_EntryReq_t* data_out) {
    if (length < 1)
        return 0;

    data_out->Target = (enum Bootloader_BootTarget) ((bytes[0] & 0x0F));
    data_out->SEQ = ((bytes[0] >> 4) & 0x0F);
    return 1;
}

int Bootloader_decode_EntryReq(const uint8_t* bytes, size_t length, enum Bootloader_BootTarget* Target_out, uint8_t* SEQ_out) {
    if (length < 1)
        return 0;

    *Target_out = (enum Bootloader_BootTarget) ((bytes[0] & 0x0F));
    *SEQ_out = ((bytes[0] >> 4) & 0x0F);
    return 1;
}

int Bootloader_get_EntryReq(Bootloader_EntryReq_t* data_out) {
    if (!(Bootloader_EntryReq_status.flags & CAN_MSG_RECEIVED))
        return 0;

    if (data_out)
        memcpy(data_out, &Bootloader_EntryReq_data, sizeof(Bootloader_EntryReq_t));

    int flags = Bootloader_EntryReq_status.flags;
    Bootloader_EntryReq_status.flags &= ~CAN_MSG_PENDING;
    return flags;
}

void Bootloader_EntryReq_on_receive(int (*callback)(Bootloader_EntryReq_t* data)) {
    Bootloader_EntryReq_status.on_receive = (void (*)(void)) callback;
}

int Bootloader_send_EntryAck_s(const Bootloader_EntryAck_t* data) {
    uint8_t buffer[1];
    buffer[0] = (data->Target & 0x0F);
    int rc = txSendCANMessage(bus_CAN1, Bootloader_EntryAck_id, buffer, sizeof(buffer));
    return rc;
}

int Bootloader_send_EntryAck(enum Bootloader_BootTarget Target) {
    uint8_t buffer[1];
    buffer[0] = (Target & 0x0F);
    int rc = txSendCANMessage(bus_CAN1, Bootloader_EntryAck_id, buffer, sizeof(buffer));
    return rc;
}

int Bootloader_send_BootloaderBeacon_s(const Bootloader_BootloaderBeacon_t* data) {
    uint8_t buffer[1];
    buffer[0] = (data->Unit & 0x0F) | ((data->State & 0x0F) << 4);
    int rc = txSendCANMessage(bus_CAN1, Bootloader_BootloaderBeacon_id, buffer, sizeof(buffer));

    if (rc == 0) {
        Bootloader_BootloaderBeacon_last_sent = txGetTimeMillis();
    }

    return rc;
}

int Bootloader_send_BootloaderBeacon(enum Bootloader_BootTarget Unit, enum Bootloader_State State) {
    uint8_t buffer[1];
    buffer[0] = (Unit & 0x0F) | ((State & 0x0F) << 4);
    int rc = txSendCANMessage(bus_CAN1, Bootloader_BootloaderBeacon_id, buffer, sizeof(buffer));

    if (rc == 0) {
        Bootloader_BootloaderBeacon_last_sent = txGetTimeMillis();
    }

    return rc;
}

int Bootloader_BootloaderBeacon_need_to_send(void) {
    return (Bootloader_BootloaderBeacon_last_sent == -1) || (txGetTimeMillis() >= (uint32_t)(Bootloader_BootloaderBeacon_last_sent + 200));
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

void Bootloader_Data_on_receive(int (*callback)(Bootloader_Data_t* data)) {
    Bootloader_Data_status.on_receive = (void (*)(void)) callback;
}

int Bootloader_send_ExitReq_s(const Bootloader_ExitReq_t* data) {
    uint8_t buffer[1];
    buffer[0] = (data->Target & 0x0F);
    int rc = txSendCANMessage(bus_UNDEFINED, Bootloader_ExitReq_id, buffer, sizeof(buffer));
    return rc;
}

int Bootloader_send_ExitReq(enum Bootloader_BootTarget Target) {
    uint8_t buffer[1];
    buffer[0] = (Target & 0x0F);
    int rc = txSendCANMessage(bus_UNDEFINED, Bootloader_ExitReq_id, buffer, sizeof(buffer));
    return rc;
}

int Bootloader_send_DataAck_s(const Bootloader_DataAck_t* data) {
    uint8_t buffer[4];
    buffer[0] = data->Address;
    buffer[1] = (data->Address >> 8);
    buffer[2] = (data->Address >> 16);
    buffer[3] = ((data->Address >> 24) & 0x3F) | ((data->Result & 0x03) << 6);
    int rc = txSendCANMessage(bus_UNDEFINED, Bootloader_DataAck_id, buffer, sizeof(buffer));
    return rc;
}

int Bootloader_send_DataAck(uint32_t Address, enum Bootloader_WriteStatus Result) {
    uint8_t buffer[4];
    buffer[0] = Address;
    buffer[1] = (Address >> 8);
    buffer[2] = (Address >> 16);
    buffer[3] = ((Address >> 24) & 0x3F) | ((Result & 0x03) << 6);
    int rc = txSendCANMessage(bus_UNDEFINED, Bootloader_DataAck_id, buffer, sizeof(buffer));
    return rc;
}

int Bootloader_decode_Handshake_s(const uint8_t* bytes, size_t length, Bootloader_Handshake_t* data_out) {
    if (length < 5)
        return 0;

    data_out->Register = (enum Bootloader_Register) ((bytes[0] & 0x0F));
    data_out->Value = bytes[1] | bytes[2] << 8 | bytes[3] << 16 | bytes[4] << 24;
    return 1;
}

int Bootloader_decode_Handshake(const uint8_t* bytes, size_t length, enum Bootloader_Register* Register_out, uint32_t* Value_out) {
    if (length < 5)
        return 0;

    *Register_out = (enum Bootloader_Register) ((bytes[0] & 0x0F));
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

void Bootloader_Handshake_on_receive(int (*callback)(Bootloader_Handshake_t* data)) {
    Bootloader_Handshake_status.on_receive = (void (*)(void)) callback;
}

int Bootloader_send_HandshakeAck_s(const Bootloader_HandshakeAck_t* data) {
    uint8_t buffer[5];
    buffer[0] = (data->Register & 0x0F) | ((data->Response & 0x0F) << 4);
    buffer[1] = data->Value;
    buffer[2] = (data->Value >> 8);
    buffer[3] = (data->Value >> 16);
    buffer[4] = (data->Value >> 24);
    int rc = txSendCANMessage(bus_UNDEFINED, Bootloader_HandshakeAck_id, buffer, sizeof(buffer));
    return rc;
}

int Bootloader_send_HandshakeAck(enum Bootloader_Register Register, enum Bootloader_HandshakeResponse Response, uint32_t Value) {
    uint8_t buffer[5];
    buffer[0] = (Register & 0x0F) | ((Response & 0x0F) << 4);
    buffer[1] = Value;
    buffer[2] = (Value >> 8);
    buffer[3] = (Value >> 16);
    buffer[4] = (Value >> 24);
    int rc = txSendCANMessage(bus_UNDEFINED, Bootloader_HandshakeAck_id, buffer, sizeof(buffer));
    return rc;
}

int Bootloader_send_SoftwareBuild_s(const Bootloader_SoftwareBuild_t* data) {
    uint8_t buffer[5];
    buffer[0] = data->CommitSHA;
    buffer[1] = (data->CommitSHA >> 8);
    buffer[2] = (data->CommitSHA >> 16);
    buffer[3] = (data->CommitSHA >> 24);
    buffer[4] = (data->DirtyRepo ? 1 : 0);
    int rc = txSendCANMessage(bus_CAN2, Bootloader_SoftwareBuild_id, buffer, sizeof(buffer));

    if (rc == 0) {
        Bootloader_SoftwareBuild_last_sent = txGetTimeMillis();
    }

    return rc;
}

int Bootloader_send_SoftwareBuild(uint32_t CommitSHA, uint8_t DirtyRepo) {
    uint8_t buffer[5];
    buffer[0] = CommitSHA;
    buffer[1] = (CommitSHA >> 8);
    buffer[2] = (CommitSHA >> 16);
    buffer[3] = (CommitSHA >> 24);
    buffer[4] = (DirtyRepo ? 1 : 0);
    int rc = txSendCANMessage(bus_CAN2, Bootloader_SoftwareBuild_id, buffer, sizeof(buffer));

    if (rc == 0) {
        Bootloader_SoftwareBuild_last_sent = txGetTimeMillis();
    }

    return rc;
}

int Bootloader_SoftwareBuild_need_to_send(void) {
    return (Bootloader_SoftwareBuild_last_sent == -1) || (txGetTimeMillis() >= (uint32_t)(Bootloader_SoftwareBuild_last_sent + 10000));
}

int Bootloader_send_SerialOutput_s(const Bootloader_SerialOutput_t* data) {
    uint8_t buffer[8];
    buffer[0] = (data->SEQ & 0x0F) | (data->Truncated ? 16 : 0) | (data->Completed ? 32 : 0) | (data->Channel ? 64 : 0) | (data->CouldNotRead ? 128 : 0);
    buffer[1] = data->Payload[0];
    buffer[2] = data->Payload[1];
    buffer[3] = data->Payload[2];
    buffer[4] = data->Payload[3];
    buffer[5] = data->Payload[4];
    buffer[6] = data->Payload[5];
    buffer[7] = data->Payload[6];
    int rc = txSendCANMessage(bus_CAN2, Bootloader_SerialOutput_id, buffer, sizeof(buffer));

    if (rc == 0) {
        Bootloader_SerialOutput_last_sent = txGetTimeMillis();
    }

    return rc;
}

int Bootloader_send_SerialOutput(uint8_t SEQ, uint8_t Truncated, uint8_t Completed, uint8_t Channel, uint8_t CouldNotRead, uint8_t* Payload) {
    uint8_t buffer[8];
    buffer[0] = (SEQ & 0x0F) | (Truncated ? 16 : 0) | (Completed ? 32 : 0) | (Channel ? 64 : 0) | (CouldNotRead ? 128 : 0);
    buffer[1] = Payload[0];
    buffer[2] = Payload[1];
    buffer[3] = Payload[2];
    buffer[4] = Payload[3];
    buffer[5] = Payload[4];
    buffer[6] = Payload[5];
    buffer[7] = Payload[6];
    int rc = txSendCANMessage(bus_CAN2, Bootloader_SerialOutput_id, buffer, sizeof(buffer));

    if (rc == 0) {
        Bootloader_SerialOutput_last_sent = txGetTimeMillis();
    }

    return rc;
}

int Bootloader_SerialOutput_need_to_send(void) {
    return (Bootloader_SerialOutput_last_sent == -1) || (txGetTimeMillis() >= (uint32_t)(Bootloader_SerialOutput_last_sent + 40));
}

void candbHandleMessage(uint32_t timestamp, int bus, CAN_ID_t id, const uint8_t* payload, size_t payload_length) {
    switch (id) {
    case Bootloader_EntryReq_id: {
        if (!Bootloader_decode_EntryReq_s(payload, payload_length, &Bootloader_EntryReq_data))
            break;

        canUpdateMsgStatusOnReceive(&Bootloader_EntryReq_status, timestamp);

        if (Bootloader_EntryReq_status.on_receive)
            ((int (*)(Bootloader_EntryReq_t*)) Bootloader_EntryReq_status.on_receive)(&Bootloader_EntryReq_data);

        break;
    }
    case Bootloader_Data_id: {
        if (!Bootloader_decode_Data_s(payload, payload_length, &Bootloader_Data_data))
            break;

        canUpdateMsgStatusOnReceive(&Bootloader_Data_status, timestamp);

        if (Bootloader_Data_status.on_receive)
            ((int (*)(Bootloader_Data_t*)) Bootloader_Data_status.on_receive)(&Bootloader_Data_data);

        break;
    }
    case Bootloader_Handshake_id: {
        if (!Bootloader_decode_Handshake_s(payload, payload_length, &Bootloader_Handshake_data))
            break;

        canUpdateMsgStatusOnReceive(&Bootloader_Handshake_status, timestamp);

        if (Bootloader_Handshake_status.on_receive)
            ((int (*)(Bootloader_Handshake_t*)) Bootloader_Handshake_status.on_receive)(&Bootloader_Handshake_data);

        break;
    }
    }
}
