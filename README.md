## funcmap PHP extension
Problem: how to detect unused/dead legacy functions in production code?
Solution: collect all the functions that ARE used and remove them from the list of functions available. What's left is unused.

funcmap is a simple PHP extension that collects names of all userspace functions that's been called and writes them down to a text file from time to time.
To reduce the resource usage, you can limit the probability of that event.

## Configuration
* **funcmap.enabled bool** (PHP\_INI\_SYSTEM) - enable/disable the extension. The extension won't do anything at all if this setting is set to false. Default value: false
* **funcmap.logfile string** (PHP\_INI\_ALL) - path to the logfile. File must be writable by PHP process user, otherwise you'll get an error in the logs. The filename may contain '%pid%' pattern, so that each child could write to its own file. Adding the '%pid%' pattern is strongly recommended for multiprocess PHP installations, otherwise you'll most likely end up with garbage results. Default value: ""
* **funcmap.probability int** (PHP\_INI\_SYSTEM) - enable data collection with this probability. This is probablity in percents, so any valuu greater than 100 will be treated as 100, i.e. "always enabled". Setting it to a value <= 0 will result in no data being written ever. Default value: 100
* **funcmap.flush\_interval\_sec int** (PHP\_INI\_ALL) - write collected data to the disk each N seconds (and on process shutdown). Zero value means "write on process shutdown only" (PHP-FPM processes are shut down, for example, when they reach their request limit). Default value: 0

## Example configuration
```
funcmap.enabled=1                            # enable the extension
funcmap.logfile=/tmp/logs/funcmap_%pid%.log  # each PHP process writes to its own log file
funcmap.flush_interval_sec=200               # write data each 200 seconds if the process lives longer than that
funcmap.probability=2                        # collect data for 2% script executions only
```

