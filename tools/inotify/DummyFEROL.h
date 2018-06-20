#ifndef _evb_test_DummyFEROL_h_
#define _evb_test_DummyFEROL_h_

#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>

#include <stdint.h>

#include "cgicc/HTMLClasses.h"
#include "evb/EvBApplication.h"
#include "evb/OneToOneQueue.h"
#include "evb/PerformanceMonitor.h"
#include "evb/dummyFEROL/Configuration.h"
#include "evb/dummyFEROL/FragmentGenerator.h"
#include "evb/dummyFEROL/StateMachine.h"
#include "toolbox/mem/Reference.h"
#include "toolbox/task/Action.h"
#include "toolbox/task/WaitingWorkLoop.h"
#include "xdaq/ApplicationStub.h"
#include "xdata/UnsignedInteger32.h"
#include "xdata/Double.h"
#include "xgi/Output.h"


namespace evb {
  namespace test {

    /**
     * \ingroup xdaqApps
     * \brief The dummy FEROL
     */

    class DummyFEROL : public EvBApplication<dummyFEROL::Configuration,dummyFEROL::StateMachine>
    {

    public:

      DummyFEROL(xdaq::ApplicationStub*);

      virtual ~DummyFEROL() {};

      XDAQ_INSTANTIATOR();

      /**
       * Configure
       */
      void configure();

      /**
       * Start triggers and send events
       */
      void startProcessing();

      /**
       * Stop triggers and drain events
       */
      void drain();

      /**
       * Stop sending events
       */
      void stopProcessing();


    private:

      virtual void do_appendApplicationInfoSpaceItems(InfoSpaceItems&);
      virtual void do_appendMonitoringInfoSpaceItems(InfoSpaceItems&);
      virtual void do_updateMonitoringInfo();
      virtual void do_handleItemChangedEvent(const std::string& item);

      virtual cgicc::table getMainWebPage() const;
      cgicc::div getHtmlSnipped() const;
      void fragmentFIFOWebPage(xgi::Input*, xgi::Output*);

      void resetMonitoringCounters();
      void getApplicationDescriptors();
      void startWorkLoops();
      bool generating(toolbox::task::WorkLoop*);
      bool sending(toolbox::task::WorkLoop*);
      void updateCounters(toolbox::mem::Reference*);
      void sendData(toolbox::mem::Reference*);
      void getPerformance(PerformanceMonitor&);

      xdaq::ApplicationDescriptor* ruDescriptor_;

      volatile bool doProcessing_;
      volatile bool generatingActive_;
      volatile bool sendingActive_;

      toolbox::task::WorkLoop* generatingWL_;
      toolbox::task::WorkLoop* sendingWL_;
      toolbox::task::ActionSignature* generatingAction_;
      toolbox::task::ActionSignature* sendingAction_;

      typedef OneToOneQueue<toolbox::mem::Reference*> FragmentFIFO;
      FragmentFIFO fragmentFIFO_;

      dummyFEROL::FragmentGenerator fragmentGenerator_;

      PerformanceMonitor dataMonitoring_;
      mutable boost::mutex dataMonitoringMutex_;

      xdata::UnsignedInteger32 stopAtEvent_;
      xdata::UnsignedInteger32 skipNbEvents_;
      xdata::UnsignedInteger32 duplicateNbEvents_;
      xdata::UnsignedInteger32 corruptNbEvents_;
      xdata::UnsignedInteger32 nbCRCerrors_;
      xdata::UnsignedInteger32 lastEventNumber_;
      xdata::Double bandwidth_;
      xdata::Double frameRate_;
      xdata::Double fragmentRate_;
      xdata::Double fragmentSize_;
      xdata::Double fragmentSizeStdDev_;

    };


  } } //namespace evb::test

#endif // _evb_test_DummyFEROL_h_

/// emacs configuration
/// Local Variables: -
/// mode: c++ -
/// c-basic-offset: 2 -
/// indent-tabs-mode: nil -
/// End: -
