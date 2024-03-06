--TEST--
Checks if the logfile is created correctly
--INI--
funcmap.enabled=on
funcmap.logfile=funcmap-%pid%.log
--FILE--
<?php
$LOG_FILENAME=str_replace('%pid%', (string)getmypid(), ini_get('funcmap.logfile'));

try {
    funcmap_enable(true);

    class Test {
        function print() {
            echo "code works\n";
        }
    }

    for ($i = 0; $i < 5; ++ $i) {
        (new Test)->print();
    }

    funcmap_enable(false);
    funcmap_flush();

    $actual = file_get_contents($LOG_FILENAME);

    echo "logfile: $LOG_FILENAME:\n";
    echo $actual;
} finally {
    // cleanup
    unlink($LOG_FILENAME);
}

?>
--EXPECTF--
code works
code works
code works
code works
code works
logfile: funcmap-%d.log:
Test::print