# FAQ

## What the hell is this folder!?

This folder contains basic gameplay infrastructure that is **optional**, but often useful — such as the abstract `GameMode` base class and the CharacterController/UpgradeManager.

## Does this belong to Orka?

Well... Obviously it does

But `GameMode` is a generic, minimal implementation. You *can* use it as a base to build your own game modes — but you don't have to! The lifecycle is `load()`, `update()`, `unload()`.

## What about this CharacterController... thing?

CharacterController and Upgrade system can be used to build your own Player class! They are more specific but still enough generic to be part of Orka!

**Nothing in this folder is required. But it can save you time!**
