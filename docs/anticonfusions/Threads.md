# Threading... also somehow anti-confusion :/

Here I show you an overview which standard threads exist in Orka, and what they do.
- Main thread: Main thread (*int main()*) is mainly used for Window creation because WindowS hates threads!
- Render thread: Used for rendering stuff. It runs as fast as your FPS. Also used to update SceneNodes/Scenes/Cameras... There might be multiple Render threads to update SceneNodes, upload meshes etc. in parallel.
- Physics thread: Simulates Physics, updates SceneNodes after Physics simulation. May also use multiple threads.
- Game thread: GameMode, networking, everything game related happens here!
- Threads you define! (AI threads, whatever)