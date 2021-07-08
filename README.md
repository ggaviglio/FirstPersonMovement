# Advanced first person movement
An example of extending Unreal Engine’s base player class and adding movement mechanics from other games. Features subtle changes to environmental collisions, in addition to some polarizing mechanics that are more “exploitable” for gaining/maintaining speed.


## Motivation
Player control and physics are cool. I wanted to explore Unreal’s player class and implement gameplay that incorporates gaining and steering player velocity. I thought it would be fun to experiment with sick momentum based movement options that interact with the environment.


## Features
* Redirect vertical velocity with “slope boosting”
* Additive jump and retained vertical velocity when leaving ramps/slopes
* Variable run speed on ramps
* Auto/Buffered “bunnyhop”
* “Fast fall” with crouch
* Crouch slide
* iD Tech/Source engine “air strafe”
* Subtle air control, disabled during air strafe


## Example
[Video][video-link]


## Setup
* Made using Unreal Engine version: 4.26.1
* Clone project and launch Abstraction.uproject


## Acknowledgements
Air Strafe implementation in Unreal Engine - [Project Borealis](https://github.com/ProjectBorealis/PBCharacterMovement)


## License
[MIT](https://choosealicense.com/licenses/mit/)


[video-link]: https://drive.google.com/file/d/1i8gei78IzKIYRb0O-ONkewZnQID7iZKn/view?usp=sharing
