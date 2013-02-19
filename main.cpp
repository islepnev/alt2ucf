//
//    Copyright 2012 Ilja Slepnev
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include <QtGui/QApplication>

#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QSettings>
#include <QTextStream>
#include <QStringList>
#include <QMap>
#include <QRegExp>

QStringList exportUcf(QTextStream &ts)
{
    QMap<QString, QString> tags;
    tags["FPGA_PINNUM"] = "LOC";
    tags["FPGA_IOSTANDARD"] = "IOSTANDARD";
    tags["FPGA_SLEW"] = "SLEW";
    tags["FPGA_DRIVE"] = "DRIVE";
    QMap< QString, QMap<QString, QString> > translateValue;
    translateValue["IOSTANDARD"]["LVDS"] = "LVDS_25";
    translateValue["IOSTANDARD"]["HSTLI_18"] = "HSTL_I_18";
    translateValue["IOSTANDARD"]["HSTLII_18"] = "HSTL_II_18";
    translateValue["IOSTANDARD"]["HSTLIII_18"] = "HSTL_III_18";
    translateValue["IOSTANDARD"]["DHSTL18I"] = "DIFF_HSTL_I_18";
    translateValue["IOSTANDARD"]["DHSTL18II"] = "DIFF_HSTL_II_18";
    translateValue["IOSTANDARD"]["DHSTL18III"] = "DIFF_HSTL_III_18";

    QStringList result;
    quint64 lineNumber = 0;
    QMap<QString, QString> record;
    while(true) {
        QString line = ts.readLine().trimmed();
        lineNumber++;
        if (line.isNull())
            break;
        if (line.isEmpty())
            continue;
        if (line.startsWith(QChar(';')))
            continue;
        record.clear();
        QStringList splitLine = line.split(QChar('|'), QString::SkipEmptyParts);
        Q_FOREACH(QString s, splitLine) {
            s = s.trimmed();
            QStringList sl = s.split(QChar('='), QString::SkipEmptyParts);
            if (sl.count() != 2) {
                qWarning() << qApp->tr("Unknown syntax on line %1: %2").arg(lineNumber).arg(s);
                continue;
            }
            record[sl[0]] = sl[1];
//            qDebug() << sl[0] << sl[1];
        }
//        qDebug() << "---";
        if ((record.value("Record") == "Constraint")
                && record.value("TargetKind") == "Port") {
            QString id = record.value("TargetId");
            QRegExp rx("(\\S+)\\[(\\d+)..(\\d+)\\]");
            QStringList idList;
            if (rx.indexIn(id) == -1) {
                idList += id;
            } else {
                int i1 = rx.cap(2).toInt();
                int i2 = rx.cap(3).toInt();
//                qDebug() << rx.cap(1) << i1 << i2;
                for(int i=i1; i>=i2; i--)
                    idList.push_back(QString("%1<%2>").arg(rx.cap(1)).arg(i));
            };

            //            QStringList padList = record.value("FPGA_PINNUM").split(',', QString::SkipEmptyParts);
            QMap<QString, QStringList> optListMap;
            Q_FOREACH(QString key, tags.keys()) {
                optListMap[key] = record.value(key).split(',', QString::SkipEmptyParts);
            }
//            qDebug() << idList;
            for(int index=0; index < idList.count(); index++) {
                QMap<QString, QString> options;
                Q_FOREACH(QString key, tags.keys()) {
                    QStringList optList = optListMap[key];
                    if (index < optList.count()) {
                        QString outTag = tags[key];
                        QString outVal = optList[index];
                        if (outTag == "IOSTANDARD") {
//                            if (outVal == "LVDS")
//                                outVal = "LVDS_25";
//                            if (outVal == "HSTLI_18")
//                                outVal = "HSTL_I_18";
                            QString outXval = translateValue["IOSTANDARD"].value(outVal);
                            if (!outXval.isNull())
                                outVal = outXval;
                        }
                        // add quotes
                        if (outTag == "LOC" && !(outVal.startsWith('"') && outVal.endsWith('"')))
                            outVal = QString("\"%1\"").arg(outVal);
                        options[outTag] = outVal;
                    }
                }
//                qDebug() << options;
//                QString pad = padList.front(); padList.pop_front();
//                if (!pad.isEmpty())
//                    options["LOC"] = pad;
                QString s = QString("NET \"%1\"").arg(idList[index]);
                QStringList sl;
                Q_FOREACH(QString key, options.keys()) {
                    sl.push_back(QString("%1 = %2").arg(key).arg(options.value(key)));
                }
                if (!sl.isEmpty())
                    s += " " + sl.join(" | ");
                result.push_back(s + ";\n");
            }
        };
    }
    return result;
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setOrganizationName("AFI Electronics");
    a.setApplicationName("Alt2ucf");
    QSettings settings;
    QString inputFileName = settings.value("inputFileName").toString();
//    QString altFileName = mruFileName;
    QString selectedInputFileName = QFileDialog::getOpenFileName(0, a.tr("Open File"),
                                                       inputFileName,
                                                       a.tr("Altium Designer Constraints (*.Constraint);;All Files (*)"));
    if (selectedInputFileName.isEmpty())
        return 0;
    QFile inputFile(selectedInputFileName);
    if (!inputFile.open(QFile::ReadOnly)) {
        qCritical() << a.tr("Can't read file %1: %2").arg(selectedInputFileName).arg(inputFile.errorString());
        return 1;
    }
    settings.setValue("inputFileName", selectedInputFileName);

// Translate Altium file to string
    QTextStream ts(&inputFile);
    QStringList outText = exportUcf(ts);
    inputFile.close();

// Write UCF file
    QString outputFileName = settings.value("outputFileName").toString();
    if (outputFileName.isEmpty())
        outputFileName = selectedInputFileName.replace(".Constraint", "", Qt::CaseInsensitive).append(".ucf");
    QString selectedOutputFileName = QFileDialog::getSaveFileName(0, a.tr("Save File"),
                                                       outputFileName,
                                                       a.tr("Xilinx Constraints (*.ucf);;All Files (*)"));
    if (selectedOutputFileName.isEmpty())
        return 0;
    settings.setValue("outputFileName", selectedOutputFileName);
    QFile outputFile(selectedOutputFileName);
    if (!outputFile.open(QFile::WriteOnly)) {
        qCritical() << a.tr("Can't write file %1: %2").arg(selectedOutputFileName).arg(outputFile.errorString());
        return 1;
    }
    QTextStream outStream(&outputFile);
    outStream << outText.join("");
    outputFile.close();
    return 0;
}
