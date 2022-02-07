# DirectSend
*Application to send files directly on another computer*
* * *

## HOW TO
DirectSend allows you to choose whether you'll be a `server` or a `client`  
To do so specify your role as the first argument: `server` or `client`

Next specify mode of usage: `send` or `receive`
If you are a `client` you need to specify `-ip` and `-port` of server
If you are a `server` you only need to specify `-port` on which connection should be established
With `send` mode you have to specify `-file` to send
With `receive` mode you can specify `-path` where to store received file, if you leave this empty it will save in current directory

## Examples:
<pre>
client receive -ip 192.168.1.1 -port 5666 -path "S:\Folder"
</pre>
<pre>
server send -port 5666 -file "S:\Folder\Archive.zip"
</pre>

