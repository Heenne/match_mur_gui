import pytest

from match_mur_gui.remote_discovery_config import RemoteDiscoveryConfig


def test_configured_host_uses_direct_discovery_and_configured_domain(tmp_path):
    env = {
        "MUR_GUI_REMOTE_ROS_HOST": "robot.lab.example",
        "MUR_GUI_REMOTE_ROS_DOMAIN_ID": "87",
    }
    config = RemoteDiscoveryConfig.from_environment(env)
    message = config.configure(
        tmp_path,
        environ=env,
        resolver=lambda host: ["10.8.0.21"],
        reachable=lambda _address: pytest.fail("direct ROS discovery must not require SSH"),
    )

    assert env["ROS_DOMAIN_ID"] == "87"
    assert "10.8.0.21" in message
    assert "domain 87" in message
    assert "direct" in message.lower()
    assert "robot.lab.example" in message


@pytest.mark.parametrize("domain", ["-1", "233", "abc"])
def test_remote_domain_is_validated(domain):
    with pytest.raises(ValueError, match="between 0 and 232"):
        RemoteDiscoveryConfig.from_environment({"MUR_GUI_REMOTE_ROS_DOMAIN_ID": domain})


def test_remote_host_cannot_embed_credentials():
    with pytest.raises(ValueError, match="host name only"):
        RemoteDiscoveryConfig.from_environment({"MUR_GUI_REMOTE_ROS_HOST": "user:secret@robot"})


def test_existing_ros_domain_takes_precedence_over_remote_default(tmp_path):
    env = {
        "ROS_DOMAIN_ID": "31",
        "MUR_GUI_REMOTE_ROS_HOST": "robot.lab.example",
        "MUR_GUI_REMOTE_ROS_DOMAIN_ID": "87",
    }
    config = RemoteDiscoveryConfig.from_environment(env)
    message = config.configure(tmp_path, environ=env, resolver=lambda _host: ["10.8.0.21"])
    assert env["ROS_DOMAIN_ID"] == "31"
    assert "domain 31" in message
