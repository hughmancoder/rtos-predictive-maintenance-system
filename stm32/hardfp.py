Import("env")

flags = ["-mfpu=fpv4-sp-d16", "-mfloat-abi=hard"]
env.Append(
    CCFLAGS=flags,
    ASFLAGS=flags,
    LINKFLAGS=flags,
)

