## Input

`InputHandler` exposes mouse, keyboard and gamepad input. Nothing else.
It offers:

### Event listeners

Register listeners with:

- `InputHandler::addMouseEventListener(...)`
- `InputHandler::addKeyboardEventListener(...)`
- `InputHandler::addControllerEventListener(...)`

Remove them with the matching `remove...Listener(id)` method, or clear everything with `removeAllEventListeners()`.

### Quick state queries
For quick-and-dirty tests (and lazy devs)

- `InputHandler::isPressed(key)` returns whether a keyboard key is currently pressed.
- `InputHandler::getMousePosition()` returns the current mouse position.
- `InputHandler::getPreviousMousePosition()` returns the previous mouse position from the last mouse event.

### Event payloads

- `MouseEvent` contains `eventType`, `x`, `y`, `lastX`, `lastY`.
- `KeyboardEvent` contains `eventType` and `key`.
- `ControllerEvent` contains `eventType`, `controllerID`, `pressed` and `value`.

`controllerID` is the SDL gamepad instance ID of the device that produced the event.

### Gamepads
- Connected gamepads are identified by their SDL instance ID btw. You can get the current IDs with `InputHandler::getConnectedGamepads()`.
- Use `InputHandler::isGamepadConnected(controllerID)` to check whether a specific gamepad is still available.

### Gamepad rumble
Because it's 100% necessary everywhere!

```cpp
std::vector<int> gamepads = InputHandler::getConnectedGamepads();
if (!gamepads.empty()) {
    const int controllerID = gamepads.front();
    InputHandler::rumbleGamepadLowFrequency(controllerID, 65535, 200);
    InputHandler::rumbleGamepadHighFrequency(controllerID, 32768, 120);
    InputHandler::rumbleGamepad(controllerID, 65535, 32768, 250);
    InputHandler::rumbleGamepadTriggers(controllerID, 65535, 120, GTR_RIGHT);
}
```

Use `stopGamepadRumble(controllerID)` and `stopGamepadTriggerRumble(controllerID)` to stop effects early.

> #### Tip:
> Because controller events include `controllerID`, you can rumble the same device that produced the input event.
