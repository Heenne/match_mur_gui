"""The MuR dialog and UWA must share the same Cartesian axis convention."""
from types import SimpleNamespace
import pytest
from match_mur_gui.base_gui import ManipulatorJogDialog


@pytest.mark.parametrize('mode,expected',[
    ('translation',[0.,-.01,0.,0.,0.,0.]),('rotation',[0.,0.,0.,0.,-.1,0.])])
def test_existing_mur_dialog_uses_shared_mapping(mode,expected):
    dialog=SimpleNamespace(arm_ready=lambda:True,mode=mode,
        linear_speed=SimpleNamespace(value=lambda:.01),angular_speed=SimpleNamespace(value=lambda:.1))
    ManipulatorJogDialog._set_axis(dialog,[0,-1,0])
    assert dialog.active==expected
