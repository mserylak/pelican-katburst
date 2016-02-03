#include "SpectrumDataSet.h"
#include "SigprocStokesWriter.h"
#include "time.h"
#include <iomanip>
#include <cmath>
#include <string>
#include <cstring>
#include <iostream>
#include <fstream>

namespace pelican {
namespace ampp {

// Constructor.
SigprocStokesWriter::SigprocStokesWriter(const ConfigNode& configNode ) : AbstractOutputStream(configNode), _first(true)
{
    _nSubbands = configNode.getOption("subbandsPerPacket", "value", "1024").toUInt();
    _nTotalSubbands = configNode.getOption("totalComplexSubbands", "value", "512").toUInt();
    _clock = configNode.getOption("clock", "value", "400").toFloat(); // Sampling clock in MHz.
    _integration = configNode.getOption("integrateTimeBins", "value", "1").toUInt();
    _integrationFreq = configNode.getOption("integrateFrequencyChannels", "value", "1").toUInt();
    _nChannels = configNode.getOption("outputChannelsPerSubband", "value", "1").toUInt();
    _nChannels = _nChannels / _integrationFreq;
    _nchans = _nChannels * _nSubbands;
    _nRawPols = configNode.getOption("nRawPolarisations", "value", "2").toUInt();
    _nBits = configNode.getOption("dataBits", "value", "32").toUInt();
    _nRange = (int) pow(2.0,(double) _nBits) - 1.0;
    _cropMin = configNode.getOption("scale", "min", "0.0" ).toFloat();
    bool goodConversion = false;
    _cropMax = configNode.getOption("scale", "max", "X" ).toFloat(&goodConversion);
    if ( ! goodConversion )
    {
        _cropMax=_nRange;
    }
    _scaleDelta = _cropMax - _cropMin;

    // Initialize connection manager thread.
    _filepath = configNode.getOption("file", "filepath");
    float subbandwidth = _clock / (_nRawPols * _nTotalSubbands); // Subband/channel bandwidth in MHz.
    //std::cout << "SigprocStokesWriter::SigprocStokesWriter(): subbandwidth " << std::fixed << std::setprecision(8) << subbandwidth << std::endl;
    float channelwidth = subbandwidth / _nChannels;
    //std::cout << "SigprocStokesWriter::SigprocStokesWriter(): channelwidth " << std::fixed << std::setprecision(8) << channelwidth << std::endl;

    // For now we will use fixed values but in the future this has to be solved (i.e. query the CAM system on KAT-7).
    if ( configNode.getOption("frequencyChannel1", "value" ) == "" )
    {
        _fch1 = 1822.0 + ((_nSubbands * channelwidth) / 2.0) - channelwidth;
    }
    else
    {
        _fch1 = configNode.getOption("frequencyChannel1", "value" , "1846.609375").toFloat();
    }
    //std::cout << "SigprocStokesWriter::SigprocStokesWriter(): frequencyChannel1 " << std::fixed << std::setprecision(8) << _fch1 << std::endl;
    if ( configNode.getOption("foff", "value" ) == "" )
    {
        _foff = -channelwidth;
    }
    else
    {
        _foff = configNode.getOption("foff", "value", "-0.390625").toFloat();
    }
    //std::cout << "SigprocStokesWriter::SigprocStokesWriter(): foff " << std::fixed << std::setprecision(8) << _foff << std::endl;
    if ( configNode.getOption("tsamp", "value" ) == "" )
    {
        _tsamp = (_nRawPols * _nTotalSubbands) * _nChannels * _integrationFreq * _integration / _clock / 1e6;
    }
    else
    {
        _tsamp = configNode.getOption("tsamp", "value", "0.00008192").toFloat();
    }
    //std::cout << "SigprocStokesWriter::SigprocStokesWriter(): tsamp " << std::fixed << std::setprecision(10) << _tsamp << std::endl;

    _nPols = configNode.getOption("params", "nPolsToWrite", "1").toUInt();
    _buffSize = configNode.getOption("params", "bufferSize", "5120").toUInt();
    _first = (configNode.hasAttribute("writeHeader") && configNode.getAttribute("writeHeader").toLower() == "true" );
    _site = configNode.getOption("TelescopeID", "value", "64").toUInt();
    _machine = configNode.getOption("MachineID", "value", "13").toUInt();
    _sourceName = "K7Field";
    _cur = 0;

    // Open file.
    _buffer.resize(_buffSize);

    // Get the present time to be used in filling file name.
    char timestr[22];
    time_t now = time(0);
    struct tm tstruct;
    tstruct = *localtime(&now);
    strftime(timestr, sizeof timestr, "D%Y%m%dT%H%M%S", &tstruct );
    QString fileName;
    fileName = _filepath + QString("_") + timestr + QString(".fil");
    _file.open(fileName.toUtf8().data(), std::ios::out | std::ios::binary);
}

void SigprocStokesWriter::writeHeader(SpectrumDataSetStokes* stokes)
{
    double _timeStamp = stokes->getLofarTimestamp();
    struct tm tm;
    time_t _epoch;

    if ( strptime("2011-1-1 0:0:0", "%Y-%m-%d %H:%M:%S", &tm) != NULL ) // MJD of 2011-01-01 is 55562.0.
    {
      _epoch = mktime(&tm);
    }
    else
    {
        throw( QString("SigprocStokesWriter: unable to set epoch.") );
    }
    double _mjdStamp = (_timeStamp-_epoch)/86400 + 55562.0;

    // Write header.
    WriteString("HEADER_START");
    WriteInt("machine_id", _machine);
    WriteInt("telescope_id", _site);
    WriteInt("data_type", 1); // Channelised data.

    // Need to be parametrised ...
    WriteString("source_name");
    WriteString(_sourceName);
    WriteDouble("src_raj", _ra);  // Write J2000 Right Ascension.
    WriteDouble("src_dej", _dec); // Write J2000 Declination.
    WriteDouble("fch1", _fch1);
    WriteDouble("foff", _foff);
    WriteInt("nchans", _nchans);
    WriteDouble("tsamp", _tsamp);
    WriteInt("nbits", _nBits);        // Only 32-bit binary data output is implemented for now.
    WriteDouble("tstart", _mjdStamp); // TODO: Extract start time from first packet
    WriteInt("nifs", int(_nPols));    // Polarisation channels.
    WriteString("HEADER_END");
    _file.flush();

}

// Destructor.
SigprocStokesWriter::~SigprocStokesWriter()
{
    _file.close();
}

// ---------------------------- Header helpers --------------------------
void SigprocStokesWriter::WriteString(QString string)
{
    int len = string.size();
    char *text = string.toUtf8().data();
    _file.write(reinterpret_cast<char *>(&len), sizeof(int));
    _file.write(reinterpret_cast<char *>(text), len);
}

void SigprocStokesWriter::WriteInt(QString name, int value)
{
    WriteString(name);
    _file.write(reinterpret_cast<char *>(&value), sizeof(int));
}

void SigprocStokesWriter::WriteDouble(QString name, double value)
{
    WriteString(name);
    _file.write(reinterpret_cast<char *>(&value), sizeof(double));
}

void SigprocStokesWriter::WriteLong(QString name, long value)
{
    WriteString(name);
    _file.write(reinterpret_cast<char *>(&value), sizeof(long));
}

// ---------------------------- Data helpers --------------------------

// Write data blob to disk.
void SigprocStokesWriter::sendStream(const QString& /*streamName*/, const DataBlob* incoming)
{
    SpectrumDataSetStokes* stokes;
    DataBlob* blob = const_cast<DataBlob*>(incoming);

    if( ( stokes = (SpectrumDataSetStokes*) dynamic_cast<SpectrumDataSetStokes*>(blob) ) )
    {
        if ( _first )
        {
            _first = false;
            writeHeader(stokes);
        }

        unsigned nSamples = stokes->nTimeBlocks();
        unsigned nSubbands = stokes->nSubbands();
        unsigned nChannels = stokes->nChannels();
        unsigned nPolarisations = stokes->nPolarisations();
        float const * data = stokes->data();

        switch (_nBits) {
            case 32: {
                    for (unsigned t = 0; t < nSamples; ++t)
                    {
                        for (unsigned p = 0; p < _nPols; ++p)
                        {
                            for (int s = nSubbands - 1; s >= 0 ; --s)
                            {
                                long index = stokes->index(s, nSubbands, p, nPolarisations, t, nChannels );
                                for(int i = 0; i < nChannels ; ++i)
                                {
                                    _file.write(reinterpret_cast<const char*>(&data[index + i]), sizeof(float));
                                }
                            }
                        }
                    }
                }
                break;
            case 8:{
                    for (unsigned t = 0; t < nSamples; ++t)
                    {
                        for (unsigned p = 0; p < _nPols; ++p)
                        {
                            for (int s = nSubbands - 1; s >= 0 ; --s)
                            {
                                long index = stokes->index(s, nSubbands, p, nPolarisations, t, nChannels );
                                for(int i = nChannels - 1; i >= 0 ; --i)
                                {
                                    int ci;
                                    _float2int(&data[index + i],&ci);
                                    _file.write((const char*)&ci,sizeof(unsigned char));
                                }
                            }
                        }
                    }
                }
                break;
            default:
                throw(QString("SigprocStokesWriter: %1 bit datafiles not yet supported"));
                break;
        }
/*
        for (unsigned t = 0; t < nSamples; ++t) {
            for (unsigned p = 0; p < _nPols; ++p) {
                for (int s = nSubbands - 1; s >= 0 ; --s) {
                    data = stokes->spectrumData(t, s, p);
                    for(int i = nChannels - 1; i >= 0 ; --i) {
                        switch (_nBits) {
                            case 32:
                                _file.write(reinterpret_cast<const char*>(&data[i]), sizeof(float));
                                break;
                            case 8:
                                int ci = ;
                                _float2int(&data[i],1,8,_scaleMin,_scaleMax,&ci);
                                _file.write((const char*)&ci,sizeof(unsigned char));
                                break;
                            default:
                                throw(QString("SigprocStokesWriter:"));
                                break;
                        }
                    }
                }
            }
        }
*/
        _file.flush();
    }
    else
    {
        std::cerr << "SigprocStokesWriter::send(): Only SpectrumDataSetStokes data can be written by the SigprocWriter" << std::endl;
        return;
    }
}

void SigprocStokesWriter::_write(char* data, size_t size)
{
    int max = _buffer.capacity() -1;
    int ptr = (_cur + size) % max;
    if( ptr <= _cur )
    {
        int dsize = max - _cur;
        std::memcpy(&_buffer[_cur], data, dsize );
        _file.write(&_buffer[0], max);
        _cur = 0; size -= dsize; data += dsize * sizeof(char);
    }
    std::memcpy( &_buffer[_cur], data, size);
    _cur=ptr;
}

void SigprocStokesWriter::_float2int(const float *f, int *i)
{
    float ftmp;
    ftmp = (*f > _cropMax)? (_cropMax) : *f;
    *i = (ftmp < _cropMin) ? 0 : (int)rint((ftmp - _cropMin) * _nRange / _scaleDelta);
}

} // namepsace lofar
} // namepsace pelican

