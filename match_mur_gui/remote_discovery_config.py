"""Environment-backed settings for connecting this GUI to a remote ROS graph."""

from dataclasses import dataclass
import os
import re


_HOST_RE = re.compile(r"^[A-Za-z0-9](?:[A-Za-z0-9.-]*[A-Za-z0-9])?$")


@dataclass(frozen=True)
class RemoteDiscoveryConfig:
    """A remote ROS host and domain, without any authentication material."""

    host: str | None = None
    domain_id: int | None = None

    @classmethod
    def from_environment(cls, environ=None):
        env = os.environ if environ is None else environ
        host = env.get("MUR_GUI_REMOTE_ROS_HOST", "").strip()
        if host and not _HOST_RE.fullmatch(host):
            raise ValueError("MUR_GUI_REMOTE_ROS_HOST must be a host name only")

        raw_domain = env.get("MUR_GUI_REMOTE_ROS_DOMAIN_ID", "").strip()
        domain_id = None
        if raw_domain:
            try:
                domain_id = int(raw_domain)
            except ValueError as exc:
                raise ValueError("remote ROS domain must be between 0 and 232") from exc
            if not 0 <= domain_id <= 232:
                raise ValueError("remote ROS domain must be between 0 and 232")
        return cls(host=host or None, domain_id=domain_id)

    def configure(self, directory, *, environ=None, resolver=None, reachable=None):
        """Prepare DDS settings before rclpy starts, using direct peer discovery."""
        from match_mur_gui.ros_discovery import configure_ros_discovery

        env = os.environ if environ is None else environ
        if self.domain_id is not None:
            env.setdefault("ROS_DOMAIN_ID", str(self.domain_id))
        hosts = [self.host] if self.host else []
        message = configure_ros_discovery(
            hosts,
            directory,
            environ=env,
            resolver=resolver,
            reachable=reachable,
            direct_hosts=hosts,
        )
        if self.host:
            route = f"remote host {self.host} via direct peer discovery"
            if "domain " in message:
                message = f"{message}; {route}"
            else:
                message = f"{message}; {route}, domain {env.get('ROS_DOMAIN_ID', '62')}"
        return message
