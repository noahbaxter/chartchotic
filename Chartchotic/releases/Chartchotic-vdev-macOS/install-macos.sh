#!/bin/bash
echo "Installing Chartchotic plugins..."
cp -R Chartchotic.vst3 ~/Library/Audio/Plug-Ins/VST3/
cp -R Chartchotic.component ~/Library/Audio/Plug-Ins/Components/
echo "Installation complete!"
echo "VST3 installed to: ~/Library/Audio/Plug-Ins/VST3/"
echo "AU installed to: ~/Library/Audio/Plug-Ins/Components/"
