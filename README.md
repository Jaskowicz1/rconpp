# rconpp
Rcon++ is a modern Source RCON library for C++, allowing people to easily use RCON however they like!

#### Library Features

- Support for Valve and non-Valve games.
- Non-blocking and blocking calls.

#### To-do

- Support for hosting an RCON server.
- Support for multiple response packets.
- Make the project header-Only (all in one file).

#### Library Usage

This library is used in:
- [Factorio-Discord-Relay Revamped](https://github.com/Jaskowicz1/fdr-remake)

If you're using this library, feel free to message me and show me, you might just get your project shown here!

# Supported Systems

### Linux and Unix

This library was originally made and tested on these operating systems and will work perfectly on these!

### Windows

Windows is not currently supported but is planned.

# Getting Started

Rcon++ can only be used by adding the header and cpp file into your source code.

From there you can easily start an RCON connection with the following:

```c++
#include <iostream>
#include <rcon.h>

int main() {
        rcon::rcon rcon_client(127.0.0.1, 27015, changeme);
        rcon_client.send_data("Hello!", 3, data_type::SERVERDATA_EXECCOMMAND, [](const rcon_response& response) {
                std::cout << "response: " << response.data << "\n";
        });
        
        return 0;
}
```

# Contributing

If you want to help out, simply make a fork and submit your PR!
Once the PR has been reviewed and accepted, we will eventually merge it into the project!

Please note that PRs may take a few days to fully review/accept, so be patient!

# Support us

If you like this project, be sure to give us a ⭐️! It really helps us out!