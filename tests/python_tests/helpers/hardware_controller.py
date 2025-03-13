from .utils import get_chip_architecture, run_shell_command

class HardwareController:
    def __init__(self):
        # Initialize the hardware controller
        self.chip_architecture = None
        self.initialize_hardware()

    def initialize_hardware(self):
        self.chip_architecture = get_chip_architecture()

    def reset(self):
        if self.chip_architecture == "blackhole":
            pass
        elif self.chip_architecture == "wormhole":
            print("Resetting card")
            run_shell_command("/home/software/syseng/wh/tt-smi -wr 0")
        else:
            print("Resetting card")
            run_shell_command("/home/software/syseng/bh/tt-smi -r 0")