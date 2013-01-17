# ./ttml-record-ALL-GOLD.sh
#
# Generates GOLD gdp files for every xml in the folder.
# Run this when your plugin is in the GOLD state.
for i in `find -name \*.xml`;
do
  oname=`echo $i | sed s/\.xml/\.GOLD\.gdp/g`
  ./ttml-record.sh $i $oname
done
