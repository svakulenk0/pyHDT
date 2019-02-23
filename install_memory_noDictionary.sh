#!/usr/bin/env bash
# scripts for automated installation

echo "Validating dependencies..."
command -v python >/dev/null 2>&1 || { echo >&2 "Python is required for the installation of pyHDT! Aborting installation..."; exit 1; }
command -v pip >/dev/null 2>&1 || { echo >&2 "pip is required for the installation of pyHDT! Aborting installation..."; exit 1; }
command -v wget >/dev/null 2>&1 || { echo >&2 "wget is required for the installation of pyHDT! Aborting installation..."; exit 1; }
command -v unzip >/dev/null 2>&1 || { echo >&2 "wget is required for the installation of pyHDT! Aborting installation..."; exit 1; }

echo "Downloading HDT..."
wget https://github.com/rdfhdt/hdt-cpp/archive/v1.3.2.zip
unzip v1.3.2.zip

echo "Patching files, loading HDT in memory and without dictionary"
cp patch_memory_noDictionary/PlainDictionary.cpp hdt-cpp-1.3.2/libhdt/src/dictionary/
cp patch_memory_noDictionary/BasicHDT.cpp hdt-cpp-1.3.2/libhdt/src/hdt
cp patch_memory_noDictionary/hdt_document_memory.cpp src/hdt_document.cpp

echo "Installing pybind11..."
pip install -r requirements.txt

echo "Installing pyHDT..."
python setup.py install

echo "Cleaning up..."
rm -rf hdt-cpp-1.3.2/
