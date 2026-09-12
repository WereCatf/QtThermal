# P3 USB Protocol

Reference for the USB protocol used by P3-series thermal cameras.

> Protocol details were established through USB traffic analysis. The camera
> does not verify command CRCs, but correct checksums are emitted regardless.

## Supported devices

| Model | VID    | PID    | Resolution | Frame payload |
|-------|--------|--------|------------|---------------|
| P3    | 0x3474 | 0x45A2 | 256 x 192  | 197,632 bytes |
| P1    | 0x3474 | 0x45C2 | 160 x 120  | 77,440 bytes  |

The frame rate is approximately 25 fps.

## USB layout

```
Interface 0:          control commands
Interface 1, alt 0:   streaming disabled
Interface 1, alt 1:   streaming enabled
```

The streaming interface is enabled by selecting alternate setting 1.

## Control transfers

| bmRequestType | bRequest | wIndex | Purpose                 |
|---------------|----------|--------|-------------------------|
| 0x41          | 0x20     | 0      | Send an 18-byte command |
| 0xC1          | 0x21     | 0      | Read response data      |
| 0xC1          | 0x22     | 0      | Read the status byte    |
| 0x40          | 0xEE     | 1      | Streaming control       |

Every command is followed by a status read. The status byte is `0x02` after a
write and `0x03` after a read.

## Command format (18 bytes)

| Offset | Size | Description              |
|--------|------|--------------------------|
| 0      | 2    | Command type (LE)        |
| 2      | 2    | Parameter (usually 0x81) |
| 4      | 2    | Register / subcommand    |
| 6      | 6    | Reserved (zero)          |
| 12     | 2    | Response length (LE)     |
| 14     | 2    | Reserved (zero)          |
| 16     | 2    | CRC16-CCITT (LE)         |

Command types:

- `0x0101` - read register
- `0x1021` - status check
- `0x012f` - stream control
- `0x0136` - shutter / NUC

CRC16-CCITT uses polynomial `0x1021` and initial value `0x0000`.

## Registers

| Register | Name         | Read size | Example content        |
|----------|--------------|-----------|------------------------|
| 0x01     | model        | 30        | "P3"                   |
| 0x02     | fw_version   | 12        | "00.00.02.17"          |
| 0x06     | part_number  | 64        | "P30-1Axxxxxxxx"       |
| 0x07     | serial       | 64        | unique serial number   |
| 0x0a     | hw_version   | 64        | "P3-00.04"             |
| 0x0f     | model_long   | 64        | long model name        |

## Streaming sequence

1. Send `start_stream` and read the status/response (the response is `0x01`
   for a fresh start and `0x35` when restarted).
2. Wait roughly one second.
3. Select alternate setting 1 on interface 1.
4. Issue the `0x40/0xEE` control transfer.
5. Wait roughly two seconds for the sensor to become ready.
6. Optionally issue a bulk read (it may time out during warm-up).
7. Send `start_stream` again and read status/response.

Stopping the stream is done by selecting alternate setting 0.

## Frame transfer

Frames are read as a stream of bulk transfers on endpoint `0x81`. A full frame
consists of:

```
start marker (12) + pixel data (frame_size) + end marker (12)
```

The frame is read in chunks until `frame_size + 24` bytes have been collected.
A chunk of exactly 12 bytes before the end of the frame indicates an end
marker, so the reader resynchronises by starting over.

### Frame markers (12 bytes)

| Offset | Size | Description                              |
|--------|------|------------------------------------------|
| 0      | 1    | Length (always 0x0C)                     |
| 1      | 1    | Sync byte, 0x8C/0x8D start, 0x8E/0x8F end |
| 2      | 4    | Counter 1 (LE), identical in both markers |
| 6      | 4    | Counter 2 (LE)                           |
| 10     | 2    | Counter 3 (LE), wraps at 2048            |

Counter 1 matching between the start and end markers validates the frame.
Counter 3 increments by roughly 40 per frame and is used to detect dropped
frames.

## Frame layout

Pixel data is `2 * (2 * sensor_h + 2) * sensor_w` bytes.

For P3 (256 x 192) the pixel buffer is 386 rows by 256 columns of 16-bit
little-endian values:

| Rows    | Contents                                        |
|---------|-------------------------------------------------|
| 0-191   | IR brightness, hardware AGC'd, low byte used    |
| 192-193 | Metadata                                        |
| 194-385 | Thermal data, raw 16-bit values                 |

## Temperature conversion

Raw values are in 1/64 Kelvin units:

```
celsius = raw / 64 - 273.15
raw     = (celsius + 273.15) * 64
```

Emissivity correction applies the Stefan-Boltzmann relation:

```
T_object = ((T_apparent^4 - (1 - e) * T_reflected^4) / e) ^ 0.25
```

## Gain modes

| Mode | Range          | Sensitivity |
|------|----------------|-------------|
| High | -20 .. 150 C   | higher      |
| Low  | 0 .. 550 C     | lower       |

## Shutter / NUC

The shutter command has no response payload. After a manual shutter the camera
sends one frame using a nonstandard segmentation; the first frame should be
read and discarded. The camera also triggers a shutter automatically about
every 90 seconds.
