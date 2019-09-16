# xgossip

A High Performance Gossip Protocol implementation of TOP Network.

## Feature highlights

+ Fully designed for Multi-Cores and Muti-Threads
+ High Performance & Low memory/cpu consumption
+ a lot of innovations on the basis of the original gossip
+ Unique in the industry
+ Bloom filter to De-weighting
+ Anti-dropping & high efficiency transport
+ iOS, Android, Windows, MacOS, Linux support

## Algorithm Description

### gossip based bloomfilter

This kind of broadcasting algorithm is more like traditional gossip broadcast, but we delete duplications in the process of broadcasting by bloomfilter to avoid Netstorm and achieve more effective broadcast on condition of limited bandwidth. According to practical testing data, the broadcast can be done when the number of receiving packets of node cluster is 5~9 times the sum of nodes.

### gossip based multi-layer & sorted select

The packet loss resistance of the above algorithm is good, but its redundancy is too high. For the network that has limited number of nodes, it can achieve mutually exclusive selection of nodes in the broadcasting process and reduce sharply bandwidth redundancy. According to practical testing data, the broadcast can be done when the number of receiving packets of node cluster is same as the sum of nodes.

### gossip based multi-layer & sorted select & bloomfilter

The above-mentioned algorithm can both exert respective advantages in certain scenarios. This kind of broadcasting algorithm is achieved by combining algorithm 1 and algorithm 3. Synergistic selection based on bloomfilter and node ID hash can support mutual exclusivity and realize high-performance broadcast. According to practical testing data, the broadcast can be done when the number of receiving packets of node cluster is 1-2 times the sum of nodes.

## Example

### C++ example 

simple Examples in tests  directory

## Contact

[TOP Network](https://www.topnetwork.org/)

## License

Copyright (c) 2017-2019 Telos Foundation & contributors

Distributed under the MIT software license, see the accompanying

file COPYING or http://www.opensource.org/licenses/mit-license.php.
