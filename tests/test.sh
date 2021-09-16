curdir=`dirname $0` 

failed=0
success=0
for file in $curdir/fixtures/*.ftl; do
    echo Starting test for $file
    $curdir/../build/tests/test $file ${file/.ftl/.json}
    if [[ $? -eq 0 ]]; then
        success=$(($success+1))
    else
        failed=$(($failed+1))
    fi
done

echo "Successes: $success"
echo "Failures: $failed"
