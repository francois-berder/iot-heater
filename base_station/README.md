# Base station

## Build instructions

Type `make` or `BUILDTYPE=debug make` to build `base_station` program.

## SMS commands

| SMS                   | Description                                 |
| --------------------- | ------------------------------------------- |
| PING                  | Reply with `PONG`                           |
| VERSION               | Reply with base station software version    |
| HEATER OFF            | Set heater state to OFF                     |
| HEATER DEFROST        | Set heater state to DEFROST                 |
| HEATER ECO            | Set heater state to ECO                     |
| HEATER COMFORT        | Set heater state to COMFORT                 |
| GET HEATER            | Reply with current heater state             |
| GET IP                | Reply with public IP address                |
| GET HEATER HISTORY    | Reply with last 16 heater commands          |
| LOCK                  | Only phones whitelisted can send SMS        |
| UNLOCK <pin>          | All phones can send SMS (default PIN: 1234) |
| ADD PHONE <number>    | Add phone number to whitelist               |
| REMOVE PHONE <number> | Remove phone number from whitelist          |

By default, anyone can send commands by SMS to the server until the `LOCK` command is sent.
The server ignores the `LOCK` command if no phones are whitelisted otherwise it would brick the server. Once locked, the server will only accept SMS from phone numbers previously whitelisted.
