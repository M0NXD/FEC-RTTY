#requires -Version 7
[CmdletBinding()]
param([string]$GuiExe,[string]$OutputDevice,[string]$EvidenceDir)
$ErrorActionPreference='Stop'
$project=Split-Path -Parent $PSScriptRoot
if(-not $GuiExe){$GuiExe=Join-Path $project 'source/build-gui/fectty-gui.exe'}
$GuiExe=(Resolve-Path $GuiExe).Path
if(-not $EvidenceDir){$EvidenceDir=Join-Path $project 'third_party/cat-acceptance/gui-tcp'}
New-Item -ItemType Directory -Force $EvidenceDir | Out-Null
$EvidenceDir=(Resolve-Path $EvidenceDir).Path
if($OutputDevice -notmatch '^portaudio:([0-9]+)$'){throw 'Provide an explicitly verified PortAudio VB-CABLE output ID.'}
$list=& (Join-Path (Split-Path $GuiExe) 'fectty-portaudio-smoke.exe') --list
if($LASTEXITCODE -ne 0 -or ($list -join "`n") -notmatch ("OUT\s+"+[regex]::Escape($OutputDevice)+': CABLE Input')){throw 'Output is not the named virtual cable; refusing playback.'}
Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Collections.Concurrent;
public sealed class CatBenchServer : IDisposable {
 public readonly ConcurrentQueue<string> Commands=new ConcurrentQueue<string>();
 public readonly string Case; public int Ptt; public int Port;
 readonly TcpListener listener; readonly Thread worker; volatile bool stop; TcpClient client;
 bool lost=false; long freq=14080000; string mode="USB";
 public CatBenchServer(string test){Case=test;Ptt=test=="external"?1:0;listener=new TcpListener(IPAddress.Loopback,0);listener.Start();Port=((IPEndPoint)listener.LocalEndpoint).Port;worker=new Thread(Run);worker.IsBackground=true;worker.Start();}
 void Run(){while(!stop){try{using(var c=listener.AcceptTcpClient()){client=c;
  using(var reader=new StreamReader(c.GetStream(),Encoding.ASCII,false,1024,true)){
   string cmd;while(!stop&&(cmd=reader.ReadLine())!=null){Commands.Enqueue(DateTime.UtcNow.ToString("o")+" "+cmd);
    string reply="RPRT 0\n";
    if(cmd=="+f")reply="get_freq:\nFrequency: "+freq+"\nRPRT 0\n";
    else if(cmd=="+m")reply="get_mode:\nMode: "+mode+"\nPassband: 2400\nRPRT 0\n";
    else if(cmd=="+t")reply="get_ptt:\nPTT: "+Ptt+"\nRPRT 0\n";
    else if(cmd.StartsWith("F "))freq=long.Parse(cmd.Substring(2));
    else if(cmd.StartsWith("M "))mode=cmd.Split(' ')[1];
    else if(cmd.StartsWith("T ")){
     int value=int.Parse(cmd.Substring(2));
     if(value!=0){Ptt=value;if(Case=="reject")reply="RPRT -1\n";
      if(Case=="lost-reply"&&!lost){lost=true;c.Close();break;}}
     else if(Case=="release-failure")reply="RPRT -1\n";else Ptt=0;
    }
    byte[] data=Encoding.ASCII.GetBytes(reply);c.GetStream().Write(data,0,data.Length);
   }
  }client=null;
 }}catch{if(stop)return;}}
 }
 public void Dispose(){stop=true;try{client?.Close();}catch{}listener.Stop();worker.Join(3000);}
}
'@
$results=@()
foreach($case in @('normal','reject','lost-reply','external','release-failure','cancel','overlong')){
 $server=[CatBenchServer]::new($case);$process=$null
 try{
  $message=if($case -eq 'cancel'){'X'*200}else{"CAT $([char]0xe9)`n"}
  $seconds=if($case -eq 'cancel'){4}else{18}
  $report=Join-Path $EvidenceDir "$case.json"
  $args=@('--autostart','--rx','none','--tx',$OutputDevice,'--rig-backend','rigctld','--rig-host','127.0.0.1','--rig-port',"$($server.Port)",'--cat-ptt','--send',$message,'--quit-after-send','--run-seconds',"$seconds",'--report',$report,'--show-radio')
  if($case -eq 'overlong'){$args+=@('--tx-limit-seconds','2')}
  $info=[Diagnostics.ProcessStartInfo]::new($GuiExe);$info.UseShellExecute=$false
  foreach($a in $args){$info.ArgumentList.Add($a)}
  $info.WorkingDirectory=Split-Path $GuiExe
  $process=[Diagnostics.Process]::Start($info)
  if(-not $process.WaitForExit(25000)){throw "CAT GUI case timed out: $case"}
  $r=Get-Content -Raw $report | ConvertFrom-Json
  $commands=@($server.Commands.ToArray());$on=@($commands|Where-Object {$_ -match ' T [123]$'}).Count
  $off=@($commands|Where-Object {$_ -match ' T 0$'}).Count
  [IO.File]::WriteAllLines((Join-Path $EvidenceDir "$case-commands.txt"),$commands)
  if($case -eq 'normal'){$ok=$process.ExitCode -eq 0 -and $r.tx_success -and $on -eq 1 -and $off -eq 1 -and $r.tx_generated_samples -gt 0 -and -not $r.cat_ptt_owned -and $server.Ptt -eq 0}
  elseif($case -eq 'release-failure'){$ok=$process.ExitCode -eq 2 -and -not $r.tx_success -and $r.cat_ptt_owned -and $r.cat_fault -and $r.cat_message -match 'unkey' -and $server.Ptt -eq 1 -and $off -ge 2}
  elseif($case -eq 'external'){$ok=$process.ExitCode -eq 2 -and $on -eq 0 -and $off -eq 0 -and $r.tx_generated_samples -eq 0 -and $server.Ptt -eq 1}
  elseif($case -eq 'cancel'){$ok=$process.ExitCode -eq 2 -and $on -eq 1 -and $off -ge 1 -and $server.Ptt -eq 0 -and -not $r.cat_ptt_owned -and $r.draft_text -ceq $message}
  elseif($case -eq 'overlong'){$ok=$process.ExitCode -eq 2 -and $on -eq 0 -and $off -eq 0 -and $r.tx_generated_samples -eq 0 -and $r.draft_text -ceq $message}
  else{$ok=$process.ExitCode -eq 2 -and $on -eq 1 -and $off -ge 1 -and $server.Ptt -eq 0 -and $r.tx_generated_samples -eq 0 -and $r.cat_fault -and -not $r.cat_ptt_owned}
  $results+=[ordered]@{case=$case;passed=$ok;exit=$process.ExitCode;ptt_on=$on;ptt_off=$off;generated_samples=$r.tx_generated_samples;radio_tx=$server.Ptt}
  Write-Output "CAT GUI $case passed=$ok exit=$($process.ExitCode) ON=$on OFF=$off samples=$($r.tx_generated_samples)"
  if(-not $ok){throw "CAT GUI assertion failed: $case"}
 }finally{if($process){if(-not $process.HasExited){$process.Kill($true)};$process.Dispose()};$server.Dispose()}
}
$results | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $EvidenceDir 'results.json') -Encoding utf8
