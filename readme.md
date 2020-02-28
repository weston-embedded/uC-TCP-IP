# uC/TCP-IP

μC/TCP-IP is a compact, reliable, high-performance TCP/IP protocol stack, optimized for embedded systems. Built from the ground up with Micrium quality, scalability, and reliability, μC/TCP-IP enables the rapid configuration of required network options to minimize time-to-market.

μC/TCP-IP allows for adjustment of the memory footprint based upon design requirements. μC/TCP-IP can be configured to include only those network components necessary to the system. When a component is not used, it is not included in the build, which saves valuable memory space.

The μC/TCP-IP design introduces the new concepts of Large and Small Buffers. A large buffer is of the size required to transport a complete Ethernet frame, which is also what other TCP/IP stacks do. But, in an embedded system, it is possible that the amount of information to transmit and receive does not require the use of a full Ethernet frame. In this case, using large buffers can waste RAM and possibly impact performance. μC/TCP-IP allows the designer to maximize the system performance by defining different sizes for small and large buffers.

The source code for μC/TCP-IP features an extremely robust and highly reliable TCP/IP solution. μC/TCP-IP is designed to be certifiable for use in avionics, compliant for use in FDA-certified devices, and in other safety-critical products.

μC/TCP-IP requires the presence of an RTOS for task scheduling and mutual exclusion. To meet this requirement, Micrium provides source code to allow network applications to readily accommodate μC/OS-II and μC/OS-III.

Note: Based on the component’s extensible kernel interface, other non-Micrium kernels can also be adapted to μC/TCP-IP, but are not supported.

## For the complete documentation, visit https://doc.micrium.com/display/ucos/