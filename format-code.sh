set -x
DIRS="./"

case $1 in
    test)
        echo "Checking code formatting..."
        for DIR in $DIRS
        do
            find $DIR -regex '.*\.[ch]p*' -exec clang-format -style=file -output-replacements-xml {} \; |
            grep -c "<replacement " > /dev/null
            if [ $? -ne 1 ]; then
                echo "Commit did not match clang-format"
                exit 1
            else
                echo "No replacements suggested. It looks OK!"
                exit 0
            fi
        done
        ;;
    *)
        echo "Running code formatting..."
        for DIR in $DIRS
        do
            find $DIR -regex '.*\.[ch]p*' -exec clang-format -style=file -i {} \;
        done
        ;;
esac
