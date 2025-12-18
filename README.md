To execute this "discoverable network" architecture in C++, we need to separate the problem into two distinct layers: Discovery (UDP) and Transport (TCP/HTTP).

Since we want to abstract this properly, we shouldn't hardcode IPs. We need a "heartbeat" system where devices announce their presence and the specific folders they are sharing.

Here is the architectural blueprint:

1. The Discovery Layer (UDP Broadcast)
This is how devices find each other without a central server.

Mechanism: UDP Broadcasting to 255.255.255.255 on a fixed port (e.g., 9000).

The "Beacon": Every few seconds, your daemon sends a small packet to the entire network.

Packet Structure: A simple struct or JSON string containing:

Device Name (e.g., "Linux-Desktop")

Service Port (The TCP port where the HTTP server is running, e.g., 8080)

Shared Folders (A list of alias names, e.g., ["docs", "movies"])

The Listener: A separate thread constantly listens on port 9000. When it hears a beacon from another IP, it adds/updates that device in a local "Peer Map" (in-memory database).

2. The Transport Layer (TCP/HTTP)
Once a device is found via UDP, actual file transfer happens here. We stick with the HTTP server (Crow) because it's reliable and handles large streams well.

Endpoints:

GET /catalog: Returns the tree structure of allowed folders.

GET /download/{folder_alias}/{filename}: Streams the file.

POST /upload/{folder_alias}: Accepts incoming files.

3. The Logic Flow
Here is how the system behaves step-by-step:

Startup:

The C++ Daemon reads a config file (e.g., config.json) defining which local paths are mapped to which public aliases.

Example Config: {"work": "/home/user/work_docs", "media": "/mnt/hdd/movies"}

The Announcement Loop (Thread A):

Every 5 seconds, it constructs a packet: MAGIC_ID|MyMacBook|8080|work,media and blasts it to the LAN.

The Discovery Loop (Thread B):

Listens for packets starting with MAGIC_ID.

Updates a thread-safe std::map<IPAddress, PeerInfo>.

Pruning: If we haven't heard from IP 192.168.1.5 in 15 seconds, remove it from the map (it went offline).

The API Server (Thread C):

When you open the frontend, it queries the Peer Map.

It displays a list: "Found 'Living Room TV' sharing 'media'".

Clicking 'media' triggers a request to Living Room TV's HTTP server to get the file list.

4. Why this approach?
Zero Configuration: You don't need to know IPs. You just launch the app, and it populates the list.

Scalable: It works for 2 devices or 50 devices.

Resilient: If a device drops off WiFi, the "Pruning" logic removes it from the list automatically.

5. Critical C++ Considerations
Since we are using C++, we have to handle the networking primitives manually (unless we use Boost.Asio).

Sockets: We will need setsockopt with SO_BROADCAST enabled for the UDP socket.

Concurrency: We need a std::mutex to protect the Peer Map because the UDP thread writes to it while the HTTP thread reads from it.

Endianness: If sharing between different architectures (e.g., an ARM Raspberry Pi and an x86 Desktop), we should stick to a text-based beacon (JSON/String) rather than raw binary structs to avoid endianness headaches.
