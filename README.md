# group_chat_simulation

Systems programming is often associated with communication as this invariably requires coordinating multiple entities that are related only based on their desire to share information. This project focuses on developing a simple chat server and client called blather. The basic usage is in two parts.

Server:

Some user starts bl-server which manages the chat "room". The server is non-interactive and will likely only print debugging output as it runs.

Clients:

Any user who wishes to chat runs bl-client which takes input typed on the keyboard and sends it to the server. The server broadcasts the input to all other clients who can respond by typing their own input.

Unlike standard internet chat services, blather will be restricted to a single Unix machine and to users with permission to read related files. However, extending the programs to run via the internet will be the subject of some discussion.

Like most interesting projects, blather utilizes a combination of many different system tools. Look for the following items to arise.

- Multiple communicating processes: clients to servers
- Communication through FIFOs
- Signal handling for graceful server shutdown
- Alarm signals for periodic behavior
- Input multiplexing with select()
- Multiple threads in the client to handle typed input versus info from the server
