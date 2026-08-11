"""KiCad discovers this package and executes the registration call.

The board guard avoids calling KiCad GUI registration when the same package is
imported by the standalone generator or unit tests.
"""

try:
    import pcbnew
except ModuleNotFoundError:
    pcbnew = None


if pcbnew is not None and pcbnew.GetBoard() is not None:
    from .action import OneAxisStepperAction

    OneAxisStepperAction().register()
