﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

using Microsoft.Build.Framework;
using Microsoft.Build.Utilities;
using System.Diagnostics;
using Microsoft.Win32;
using System.Threading;

namespace EpochVS
{
    public abstract class BuildTaskCommonImplementation : Task
    {
        private string m_fileName;
        private string m_outputName;

        [Required]
        public string Filename
        {
            get { return m_fileName; }
            set { m_fileName = value; }
        }

        [Required]
        public string Output
        {
            get { return m_outputName; }
            set { m_outputName = value; }
        }

        protected void LogMissingCompiler()
        {
            Log.LogError("Compiler not found. Please ensure Epoch compiler is properly installed.");
        }

        protected abstract string GetCompilerRelativePath();

        public override bool Execute()
        {
            string compilerPath = "";

            var regvalue = Registry.GetValue("HKEY_LOCAL_MACHINE\\Software\\Epoch\\CurrentInstall", "InstallPath", "");
            if (regvalue == null)
            {
                LogMissingCompiler();
                return false;
            }

            compilerPath = regvalue as string;
            if (compilerPath.Length <= 0)
            {
                LogMissingCompiler();
                return false;
            }

            string compilerFileName = compilerPath + GetCompilerRelativePath();
            try
            {
                var process = new Process
                {
                    StartInfo = new ProcessStartInfo
                    {
                        FileName = compilerFileName,
                        Arguments = "/files " + m_fileName + " /output " + m_outputName,
                        UseShellExecute = false,
                        RedirectStandardOutput = true,
                        RedirectStandardError = true,
                        CreateNoWindow = true
                    }
                };

                using (AutoResetEvent outputHandle = new AutoResetEvent(false))
                using (AutoResetEvent errorHandle = new AutoResetEvent(false))
                {
                    process.OutputDataReceived += (sender, args) => { ProcessLine(args.Data, outputHandle); };
                    process.ErrorDataReceived += (sender, args) => { ProcessLine(args.Data, errorHandle); };

                    process.Start();

                    process.BeginOutputReadLine();
                    process.BeginErrorReadLine();

                    process.WaitForExit();
                    if (outputHandle.WaitOne(1000) && errorHandle.WaitOne(1000))
                        return (process.ExitCode == 0);
                }

                return false;
            }
            catch
            {
                Log.LogError("Error invoking compiler at \"{0}\". Please ensure Epoch compiler is properly installed.", compilerFileName);
                return false;
            }
        }

        private void ProcessLine(string data, AutoResetEvent e)
        {
            if (data != null)
                Log.LogMessage(MessageImportance.High, data);
            else
                e.Set();
        }
    }

    public class BuildTask : BuildTaskCommonImplementation
    {
        protected override string GetCompilerRelativePath()
        {
            return "\\EpochNativeBin.exe";
        }
    }


    public class BuildTask32 : BuildTaskCommonImplementation
    {
        protected override string GetCompilerRelativePath()
        {
            return "\\Epoch32\\EpochCompiler.exe";
        }
    }
}