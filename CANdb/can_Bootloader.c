#include "can_Bootloader.h"
#include <string.h>

CAN_msg_status_t Bootloader_EntryReq_status;
Bootloader_EntryReq_t Bootloader_EntryReq_data;
int32_t Bootloader_SoftwareBuild_last_sent;
int32_t Bootloader_SerialOutput_last_sent;

void candbInit(void) {
    canInitMsgStatus(&Bootloader_EntryReq_status, -1);
    Bootloader_SoftwareBuild_last_sent = -1;
    Bootloader_SerialOutput_last_sent = -1;
}

int Bootloader_decode_EntryReq_s(const uint8_t* bytes, size_t length, Bootloader_EntryReq_t* data_out) {
    if (length < 1)
        return 0;

    data_out->SEQ = (bytes[0] & 0x3F);
    data_out->Target = (enum Bootloader_BootTarget) (((bytes[0] >> 6) & 0x03));
    return 1;
}

int Bootloader_decode_EntryReq(const uint8_t* bytes, size_t length, uint8_t* SEQ_out, enum Bootloader_BootTarget* Target_out) {
    if (length < 1)
        return 0;

    *SEQ_out = (bytes[0] & 0x3F);
    *Target_out = (enum Bootloader_BootTarget) (((bytes[0] >> 6) & 0x03));
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
    buffer[0] = (data->Target & 0x03);
    int rc = txSendCANMessage(bus_CAN1, Bootloader_EntryAck_id, buffer, sizeof(buffer));
    return rc;
}

int Bootloader_send_EntryAck(enum Bootloader_BootTarget Target) {
    uint8_t buffer[1];
    buffer[0] = (Target & 0x03);
    int rc = txSendCANMessage(bus_CAN1, Bootloader_EntryAck_id, buffer, sizeof(buffer));
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
    }
}
