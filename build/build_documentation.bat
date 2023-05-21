@echo off
python "mcss\documentation\doxygen.py" "mcssconf.py"
rmdir /s /q "..\documentation\xml"
