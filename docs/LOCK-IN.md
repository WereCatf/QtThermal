# Lock-In Thermography

Lock-in thermography resolves very small temperature changes, even below the
noise floor of the camera, by integrating the response to a periodic stimulus
over a long period.

QtThermal implements this by toggling an external load while integrating the
camera signal. The result is shown as a 2x2 grid of panes next to the main
image:

```
[ In-phase     | Amplitude ]
[ Quadrature   | Phase     ]
```

## Required hardware

An external controller is needed to switch the power delivered to the device
under test. Any controller that reads a `1` or `0` from a serial port and
drives a GPIO (for example an ESP32) will do. The GPIO can switch a MOSFET or
similar to control the load.

## Configuration

Open `Lock-In -> Configure...` to set:

| Option       | Default      | Description                          |
|--------------|--------------|--------------------------------------|
| Serial port  | platform     | Serial port of the controller        |
| Baud rate    | 115200       | Serial baud rate                     |
| Period       | 1.0 s        | Integration cycle period             |
| Integration  | 60.0 s       | Total integration time               |
| Invert       | off          | Invert the control output logic      |

The controller writes `1\n` while the load is on and `0\n` while it is off.

## Running

1. Make sure the camera is streaming.
2. Choose `Lock-In -> Start`.
3. The control output toggles and the demodulated panes update in real time.
4. Choose `Lock-In -> Stop` to finish; the final result remains visible.

Frame timestamps are used to weight each frame by the sine and cosine of the
drive phase. The period and integration time are aligned to the frame tick
rate so that the drive and the integration stay in sync.
