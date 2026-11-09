Import("env")

# Arduino-Pico WiFiClientSecure hides a base write overload in dependency headers.
# Keep this C++-only: GCC 16 warns if passed this option for a C compilation.
env.Append(CXXFLAGS=["-Wno-overloaded-virtual"])
