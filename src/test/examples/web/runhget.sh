RWQS=`which runhget.sh`
if [ ${#RWQS} == 0 ]
then
DIR=.
else
DIR=`dirname $RWQS `
fi
pushd ${DIR}/WebQueryServer 
CLASSPATH=WebQueryServer.jar:gnu-regexp-1.1.4.jar:libreadline-java.jar
java  -classpath $CLASSPATH HtmlGet.HtmlGet
popd
