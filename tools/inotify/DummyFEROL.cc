#include "i2o/Method.h"
#include "i2o/utils/AddressMap.h"
#include "interface/evb/i2oEVBMsgs.h"
#include "interface/shared/fed_trailer.h"
#include "interface/shared/ferol_header.h"
#include "interface/shared/i2oXFunctionCodes.h"
#include "evb/DummyFEROL.h"
#include "evb/dummyFEROL/States.h"
#include "evb/dummyFEROL/StateMachine.h"
#include "evb/Exception.h"
#include "evb/version.h"
#include "toolbox/task/WorkLoopFactory.h"
#include "xcept/tools.h"

#include <algorithm>
#include <pthread.h>

evb::test::DummyFEROL::DummyFEROL(xdaq::ApplicationStub* app) :
  EvBApplication<dummyFEROL::Configuration,dummyFEROL::StateMachine>(app,"/evb/images/ferol64x64.gif"),
  doProcessing_(false),
  generatingActive_(false),
  fragmentFIFO_(this,"fragmentFIFO")
{
  stateMachine_.reset( new dummyFEROL::StateMachine(this) );

  resetMonitoringCounters();
  startWorkLoops();

  initialize();

  LOG4CPLUS_INFO(getApplicationLogger(), "End of constructor");
}


void evb::test::DummyFEROL::do_appendApplicationInfoSpaceItems
(
  InfoSpaceItems& appInfoSpaceParams
)
{
  stopAtEvent_ = 0;
  skipNbEvents_ = 0;
  duplicateNbEvents_ = 0;
  corruptNbEvents_ = 0;
  nbCRCerrors_ = 0;

  appInfoSpaceParams.add("lastEventNumber", &lastEventNumber_);
  appInfoSpaceParams.add("stopAtEvent", &stopAtEvent_);
  appInfoSpaceParams.add("skipNbEvents", &skipNbEvents_);
  appInfoSpaceParams.add("duplicateNbEvents", &duplicateNbEvents_);
  appInfoSpaceParams.add("corruptNbEvents", &corruptNbEvents_);
  appInfoSpaceParams.add("nbCRCerrors", &nbCRCerrors_);
}


void evb::test::DummyFEROL::do_appendMonitoringInfoSpaceItems
(
  InfoSpaceItems& monitoringParams
)
{
  lastEventNumber_ = 0;
  bandwidth_ = 0;
  frameRate_ = 0;
  fragmentRate_ = 0;
  fragmentSize_ = 0;
  fragmentSizeStdDev_ = 0;

  monitoringParams.add("lastEventNumber", &lastEventNumber_);
  monitoringParams.add("bandwidth", &bandwidth_);
  monitoringParams.add("frameRate", &frameRate_);
  monitoringParams.add("fragmentRate", &fragmentRate_);
  monitoringParams.add("fragmentSize", &fragmentSize_);
  monitoringParams.add("fragmentSizeStdDev", &fragmentSizeStdDev_);
}


void evb::test::DummyFEROL::do_updateMonitoringInfo()
{
  boost::mutex::scoped_lock sl(dataMonitoringMutex_);
  bandwidth_ = dataMonitoring_.bandwidth();
  frameRate_ = dataMonitoring_.i2oRate();
  fragmentRate_ = dataMonitoring_.logicalRate();
  fragmentSize_ = dataMonitoring_.size();
  fragmentSizeStdDev_ = dataMonitoring_.sizeStdDev();
  dataMonitoring_.reset();
}


void evb::test::DummyFEROL::do_handleItemChangedEvent(const std::string& item)
{
  if (item == "maxTriggerRate")
  {
    fragmentGenerator_.setMaxTriggerRate(this->configuration_->maxTriggerRate);
  }
}


cgicc::table evb::test::DummyFEROL::getMainWebPage() const
{
  using namespace cgicc;

  table layoutTable;
  layoutTable.set("class","xdaq-evb-layout");
  layoutTable.add(colgroup()
                  .add(col())
                  .add(col().set("class","xdaq-evb-arrow"))
                  .add(col()));
  layoutTable.add(tr()
                  .add(td(this->getWebPageBanner()))
                  .add(td(" "))
                  .add(td(" ")));
  layoutTable.add(tr()
                  .add(td(getHtmlSnipped()).set("class","xdaq-evb-component"))
                  .add(td(" "))
                  .add(td(" ")));

  return layoutTable;
}


cgicc::div evb::test::DummyFEROL::getHtmlSnipped() const
{
  using namespace cgicc;

  cgicc::div div;
  div.add(p("FEROL"));

  {
    table table;

    boost::mutex::scoped_lock sl(dataMonitoringMutex_);

    table.add(tr()
              .add(td("last event number"))
              .add(td(boost::lexical_cast<std::string>(lastEventNumber_.value_))));
    {
      std::ostringstream str;
      str.setf(std::ios::fixed);
      str.precision(2);
      str << bandwidth_.value_ / 1e6;
      table.add(tr()
                .add(td("throughput (MB/s)"))
                .add(td(str.str())));
    }
    {
      std::ostringstream str;
      str.setf(std::ios::scientific);
      str.precision(2);
      str << frameRate_.value_ ;
      table.add(tr()
                .add(td("rate (frame/s)"))
                .add(td(str.str())));
    }
    {
      std::ostringstream str;
      str.setf(std::ios::scientific);
      str.precision(2);
      str << fragmentRate_.value_ ;
      table.add(tr()
                .add(td("rate (fragments/s)"))
                .add(td(str.str())));
    }
    {
      std::ostringstream str;
      str.setf(std::ios::fixed);
      str.precision(1);
      str << fragmentSize_.value_ / 1e3 << " +/- " << fragmentSizeStdDev_.value_ / 1e3;
      table.add(tr()
                .add(td("FED size (kB)"))
                .add(td(str.str())));
    }
    div.add(table);
  }

  div.add(fragmentFIFO_.getHtmlSnipped());

  return div;
}


void evb::test::DummyFEROL::resetMonitoringCounters()
{
  boost::mutex::scoped_lock sl(dataMonitoringMutex_);
  dataMonitoring_.reset();
  lastEventNumber_ = 0;
}


void evb::test::DummyFEROL::configure()
{
  fragmentFIFO_.clear();
  fragmentFIFO_.resize(configuration_->fragmentFIFOCapacity);

  fragmentGenerator_.configure(
    configuration_->fedId,
    configuration_->usePlayback,
    configuration_->playbackDataFile,
    configuration_->frameSize,
    configuration_->fedSize,
    configuration_->computeCRC,
    configuration_->useLogNormal,
    configuration_->fedSizeStdDev,
    configuration_->minFedSize,
    configuration_->maxFedSize,
    configuration_->frameSize*configuration_->fragmentFIFOCapacity,
    configuration_->fakeLumiSectionDuration,
    configuration_->maxTriggerRate
  );

  getApplicationDescriptors();

  resetMonitoringCounters();
}


void evb::test::DummyFEROL::getApplicationDescriptors()
{
  try
  {
    ruDescriptor_ =
      getApplicationContext()->
      getDefaultZone()->
      getApplicationDescriptor(configuration_->destinationClass,configuration_->destinationInstance);
  }
  catch(xcept::Exception &e)
  {
    std::ostringstream oss;
    oss << "Failed to get application descriptor of the destination";
    XCEPT_RETHROW(exception::Configuration, oss.str(), e);
  }
}


void evb::test::DummyFEROL::startProcessing()
{
  resetMonitoringCounters();
  stopAtEvent_ = 0;
  doProcessing_ = true;
  fragmentGenerator_.reset();
  generatingWL_->submit(generatingAction_);
  sendingWL_->submit(sendingAction_);
}


void evb::test::DummyFEROL::drain()
{
  if ( stopAtEvent_.value_ == 0 ) stopAtEvent_.value_ = lastEventNumber_;
  while ( generatingActive_ || !fragmentFIFO_.empty() || sendingActive_ ) ::usleep(1000);
}


void evb::test::DummyFEROL::stopProcessing()
{
  doProcessing_ = false;
  while ( generatingActive_ || sendingActive_ ) ::usleep(1000);
  fragmentFIFO_.clear();
}


void evb::test::DummyFEROL::startWorkLoops()
{
  try
  {
    generatingWL_ = toolbox::task::getWorkLoopFactory()->
      getWorkLoop( getIdentifier("generating"), "waiting" );

    if ( ! generatingWL_->isActive() )
    {
      generatingAction_ =
        toolbox::task::bind(this, &evb::test::DummyFEROL::generating,
                            getIdentifier("generatingAction") );

      generatingWL_->activate();
    }
  }
  catch(xcept::Exception& e)
  {
    std::string msg = "Failed to start workloop 'Generating'";
    XCEPT_RETHROW(exception::WorkLoop, msg, e);
  }

  try
  {
    sendingWL_ = toolbox::task::getWorkLoopFactory()->
      getWorkLoop( getIdentifier("sending"), "waiting" );

    if ( ! sendingWL_->isActive() )
    {
      sendingAction_ =
        toolbox::task::bind(this, &evb::test::DummyFEROL::sending,
                            getIdentifier("sendingAction") );

      sendingWL_->activate();
    }
  }
  catch(xcept::Exception& e)
  {
    std::string msg = "Failed to start workloop 'Sending'";
    XCEPT_RETHROW(exception::WorkLoop, msg, e);
  }
}


bool evb::test::DummyFEROL::generating(toolbox::task::WorkLoop *wl)
{
  generatingActive_ = true;

  try
  {
    while ( doProcessing_ &&
            (stopAtEvent_.value_ == 0 || lastEventNumber_.value_ < stopAtEvent_.value_) )
    {
      toolbox::mem::Reference* bufRef = 0;

      if ( fragmentGenerator_.getData(bufRef,stopAtEvent_.value_,lastEventNumber_.value_,
                                      skipNbEvents_.value_,duplicateNbEvents_.value_,
                                      corruptNbEvents_.value_,nbCRCerrors_.value_) )
      {
        fragmentFIFO_.enqWait(bufRef);
      }
    }
  }
  catch(xcept::Exception &e)
  {
    generatingActive_ = false;
    stateMachine_->processFSMEvent( Fail(e) );
  }
  catch(std::exception& e)
  {
    generatingActive_ = false;
    XCEPT_DECLARE(exception::DummyData,
                  sentinelException, e.what());
    stateMachine_->processFSMEvent( Fail(sentinelException) );
  }
  catch(...)
  {
    generatingActive_ = false;
    XCEPT_DECLARE(exception::DummyData,
                  sentinelException, "unkown exception");
    stateMachine_->processFSMEvent( Fail(sentinelException) );
  }

  generatingActive_ = false;

  return doProcessing_;
}


bool __attribute__((optimize("O0")))  // Optimization causes segfaults as bufRef is null
evb::test::DummyFEROL::sending(toolbox::task::WorkLoop *wl)
{
  sendingActive_ = true;

  try
  {
    toolbox::mem::Reference* bufRef = 0;

    while ( fragmentFIFO_.deq(bufRef) )
    {
      updateCounters(bufRef);
      sendData(bufRef);
    }
  }
  catch(xcept::Exception &e)
  {
    sendingActive_ = false;
    stateMachine_->processFSMEvent( Fail(e) );
  }

  sendingActive_ = false;

  ::usleep(10);

  return doProcessing_;
}


inline void evb::test::DummyFEROL::updateCounters(toolbox::mem::Reference* bufRef)
{
  boost::mutex::scoped_lock sl(dataMonitoringMutex_);

  const uint32_t payload = bufRef->getDataSize();

  ++dataMonitoring_.i2oCount;
  dataMonitoring_.logicalCount += configuration_->frameSize/(configuration_->fedSize+sizeof(ferolh_t));
  dataMonitoring_.sumOfSizes += payload;
  dataMonitoring_.sumOfSquares += payload*payload;
}


inline void evb::test::DummyFEROL::sendData(toolbox::mem::Reference* bufRef)
{
  getApplicationContext()->
    postFrame(
      bufRef,
      getApplicationDescriptor(),
      ruDescriptor_
    );
}


/**
 * Provides the factory method for the instantiation of DummyFEROL applications.
 */
XDAQ_INSTANTIATOR_IMPL(evb::test::DummyFEROL)



/// emacs configuration
/// Local Variables: -
/// mode: c++ -
/// c-basic-offset: 2 -
/// indent-tabs-mode: nil -
/// End: -
