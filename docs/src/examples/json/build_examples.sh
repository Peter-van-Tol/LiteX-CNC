for f in ./*.json; 
do 
    litexcnc build_firmware $f -a --build; 
done