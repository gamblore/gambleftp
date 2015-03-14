A cross platform implementation of a server and client in a single executable. Designed for proprietary hand held devices like the Sony Playstation Portable and Nintendo DS. The point of this project was not security but implemention based on standards set forth by RFC 959 and RFC 3659. Porting the project to other platforms requires implementing only a few simple functions mostly related to how to draw the information.

# Special features #

## Single threaded server ##
This application operates in a single thread and is very portable because of this.

## DS ##
The client makes use of the touch screen to generate iPod touch like navigation. This project uses a custom compiled dswifi library. It required several patches to function properly. I hope to have them available here shortly for those that wish to compile from source.

## PSP ##
OS utility threads are used to increase the quality of the user interface.