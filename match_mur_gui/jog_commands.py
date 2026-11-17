"""Qt-independent Cartesian jogging shared by MuR and stationary UR clients."""
import math


def cartesian_twist(vector,mode,linear_speed,angular_speed):
    if mode not in ('translation','rotation'):raise ValueError('Unknown jog mode')
    if len(vector)!=3 or not all(math.isfinite(float(v)) for v in vector):
        raise ValueError('Jog direction must contain three finite values')
    speed=float(linear_speed if mode=='translation' else angular_speed)
    if not math.isfinite(speed) or speed<0:raise ValueError('Jog speed must be finite and nonnegative')
    values=[0.]*6;offset=0 if mode=='translation' else 3
    for index,value in enumerate(vector):values[offset+index]=float(value)*speed
    return values
