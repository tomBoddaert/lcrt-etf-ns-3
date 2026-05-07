# ns-3 Integration of LCRT and ETF
This library integrates LCRT and ETF into the ns-3 network simulator. For more detail on the wider project, see the [main repository][main].

## Installing
Building requires an up-to-date installation of the [cargo package manager](https://doc.rust-lang.org/cargo/). This can be installed using [rustup](https://rustup.rs/).

This repository should be cloned into your ns-3 installation's `contrib/` directory. From within the cloned repo, the submodules must be updated, which can be done with the following command.
```sh
git submodule update --init --recursive
```

The patch in [`onoff-application.cc.patch`](onoff-application.cc.patch) must be applied to the ns-3 installation manually (only a 1 character change) or by using the patch command. This fixes what we believe is a faulty assumption in ns-3's `OnOffApplication`, which we use for testing.

## Usage
The [helper modules](#helper) should be used to install LCRT and ETF on nodes. See the [examples](#examples) for installation code. Importantly, one node must be set as the source using the following code or equivalent.
```cpp
sourceNode->GetObject<lcrt::RoutingProtocol>()->SetIsSource(true);
```

## Files and Exported Types
All types are included in the `ns3::lcrt` or `ns3::etf` namespaces.

### `model/`
This contains the simulation modules.

`model/lcrt.h` and `model/lcrt.cc` define and implement the LCRT `RoutingProtocol`, extending `Ipv4RoutingProtocol`.

`model/etf.h` and `model/etf.cc` define and implement the `EtfPathing` application, extending `Application`.

### `helper/`
This contains helpers to install the simulation modules on nodes. This is the recommended way to add LCRT and ETF to a simulation.

`helper/lcrt-helper.h` and `helper/lcrt-helper.cc` define and implement the `LcrtHelper`, extending `Ipv4RoutingHelper`.

`helper/etf-helper.h` and `helper/etf-helper.cc` define and implement the `EtfHelper`, extending `ApplicationHelper`.

### `examples/`
Two examples are included; both have a command line. Command line arguments can be displayed with `./ns3 run <target> -- --help`.

`examples/benchmark.cc` (target `lcrt-benchmark`) was used to evaluate our system's runtime speed when streaming against basic static routing.

`examples/lcrt-example.cc` (target `lcrt-example`) runs LCRT on a tree of nodes, and uses ETF to move one node.

## License
This library is licensed under the [GPL-2.0 license](LICENSE_GPL-2.0).

See the [main project repository][main] for the platform-independent library license.

[main]: https://github.com/tomBoddaert/lcrt-etf
