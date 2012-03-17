/*!
#
# Win-Widget. Windows related software for Audio-Widget/SDR-Widget (http://code.google.com/p/sdr-widget/)
# Copyright (C) 2012 Nikolay Kovbasa
#
# Permission to copy, use, modify, sell and distribute this software 
# is granted provided this copyright notice appears in all copies. 
# This software is provided "as is" without express or implied
# warranty, and with no claim as to its suitability for any purpose.
#
#----------------------------------------------------------------------------
# Contact: nikkov@gmail.com
#----------------------------------------------------------------------------
*/
// Widget control utility
//

using System;
using System.Collections.Generic;
using System.Windows.Forms;

namespace WidgetControl
{
    static class Program
    {
        /// <summary>
        /// Главная точка входа для приложения.
        /// </summary>
        [STAThread]
        static void Main(string[] args)
        {
            if (args.Length != 0)
            {
                WidgetControl widCtrl = new WidgetControl();
                if (!widCtrl.InitializeDevice())
                {
                    MessageBox.Show(widCtrl.GetInfo(), "Error!");
                    return;
                }
                if (!widCtrl.SetFeatureFromFile(args[0]))
                {
                    MessageBox.Show(widCtrl.GetInfo(), "Error!");
                }
                MessageBox.Show("Success", "Information");
            }
            else
            {
                Application.EnableVisualStyles();
                Application.SetCompatibleTextRenderingDefault(false);
                Application.Run(new WidgetControl());
            }
        }
    }
}
