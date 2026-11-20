"""Command construction for an independently managed Gazebo Sim GUI client."""


def gazebo_client_command(executable="gz"):
    """Return the Harmonic command that joins a running server without starting one."""
    return [executable, "sim", "-g"]
