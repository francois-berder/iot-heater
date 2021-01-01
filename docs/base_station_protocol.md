# Message protocol specifications

The base station communicates with heater controllers using a simple protocol.
The protocol consists of fixed size messages (64 bytes). Each message first starts by a header (16 bytes in size) leaving only 48 bytes for message-specific data. All unused bytes must be set to 0xFF.

## Message header

### Message header layout

| Parameter        | Size (bytes) |
| ---------------- | -----------: |
| Protocol version |            1 |
| Type             |            1 |
| MAC address      |            6 |
| Counter          |            8 |

For now, the protocol version is 1.

The counter is present to allow the base station to detect if a heater controller rebooted. It should be initialized randomly at boot and incremented whenever a message is sent.

### Message types

| Type                 | Value       |
| -------------------- | ----------: |
| `REQ_HEATER_STATE`   |           1 |
| `HEATER_STATE_REPLY` |           2 |


## `REQ_HEATER_STATE` message

This message may contain a NULL-terminated string that represents the name of the heater controller, making it possible for the base station to send a specific state to each heater.

## `HEATER_STATE_REPLY` message

This message contains only one parameter of size one byte that indicates the heater state. It can take the following values:

| Heater state | Value |
| ------------ | ----: |
| OFF          |     0 |
| DEFROST      |     1 |
| ECO          |     2 |
| COMFORT/ON   |     3 |
