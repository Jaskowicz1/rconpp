# rcon++
Rcon++ is a modern Source RCON library for C++, allowing people to easily use RCON however they like!

#### Library Features

- Support for Valve and non-Valve games.
- Callbacks, allowing non-blocking calls.
- Support for hosting an RCON server.

#### To-do

- Support for multiple response packets.

#### Library Usage

This library is used in:
- [Factorio-Discord-Relay Revamped](https://github.com/Jaskowicz1/fdr-remake)
- RCON-UE

If you're using this library, feel free to message me and show me, you might just get your project shown here!

# Supported Systems

### Linux and Unix

This library was originally made and tested on Linux & UNIX (more specifically OSX) and will work perfectly on these!

### Windows

Windows is fully supported and tested! We natively support MSVC and clang-cl, but not MinGW.
We do not test support for MinGW, nor do we want to actively try and support it. You can try MinGW at your own risk.

# Getting Started

rcon++ can be installed from the releases section!

We're aiming to start rolling out to package managers soon!

# Quick Example

### Client
```c++
#include <iostream>
#include <rconpp/rcon.h>

int main() {
        rconpp::rcon_client client("127.0.0.1", 27015, "changeme");
	
        client.on_log = [](const std::string_view& log) {
                std::cout << log << "\n";
        };

        client.start(true);
	
        client.send_data("Hello!", 3, rconpp::data_type::SERVERDATA_EXECCOMMAND, [](const rconpp::response& response) {
                std::cout << "response: " << response.data << "\n";
        });
        
        return 0;
}
```

### Server
```c++
#include <iostream>
#include <rconpp/rcon.h>

int main() {
        rconpp::rcon_server server("0.0.0.0", 27015, "testing");

        server.on_log = [](const std::string_view log) {
            std::cout << log << "\n";
        };

        server.on_command = [](const rconpp::client_command& command) {
                if (command.command == "/test") {
                        return "This is a test!";
                } else {
                        return "Hello!";
                }
        };

        server.start(false);
        
        return 0;
}
```

# Contributing

If you want to help out, simply make a fork and submit your PR!
Once the PR has been reviewed and accepted, we will eventually merge it into the project!

Please note that PRs may take a few days to fully review/accept, so be patient!

# Support us

If you like this project, be sure to give us a ⭐️! It really helps us out!
