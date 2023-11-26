# Building this documentation

This documentation is built using Doxygen, Sphinx, Breathe and Exhale.
You can install these dependencies using the following commands:

```bash
sudo apt install doxygen
conda create -n python=3.10
conda activate doxygen
python3 -m pip install sphinx breathe exhale sphinx_rtd_theme
```

Then, you can build the documentation using the following commands:

```bash
# in the root of the repository
cd docs
make html
```

The documentation will be built in the `docs/build/html` directory.

If you want to see the coverage of the documentation, you can install the `coverxygen` tool:

```bash
python3 -m pip install coverxygen
```

Then, you can run the following command to see the coverage of the documentation:

```bash
# in the root of the repository
cd docs
python3 -m coverxygen --xml-dir _doxygen/xml --src-dir ../src --output doc-coverage.info --format markdown-summary
```
