Each morning when I take my son to school we play a little guessing
game.  When we leave the house he writes down the date, temperature,
day of the week, our leave time, and then both of our guesses in terms
of how long the commute to school will take (for us generally around
25 minutes).  Once we get to school, he writes down the actual time it
took, who won/lost, and then the average miles per gallon and the
average miles per hour for the commute.  Once a year we then do an
analysis of the data.

The idea of this Arduino application is to automate as much of that as
possible in a single device.  The device is based on an Arduino Mega
and uses the following components (thus far):
- Start/pause, stop, and cancel buttons that are used to record the trip
- An led that indicates the trip status (recording, paused, cancelled, stopped)
- A temperature/humidity sensor
- A date/time sensor
- A keypad to enter in the guesses
- A LCD to display all the data
- A microSD card to record each day's data
