PluginPath="C:\GitHub\NaturalGroundingPlayer\NaturalGroundingPlayer\bin\Debug\Encoder\"
#LoadPlugin("C:\GitHub\AviSynthShader\Src\Debug\Shader.dll")
LoadPlugin("Shader.dll")
Import("Shader.avsi")

#SetMTMode(3,8)
AviSource("F:\AVSMeter\Preview.avi", audio=false, pixel_type="YV12")
#SetMTMode(2)
SuperXBR(MatrixIn="601", fWidth=800, fHeight=600)
