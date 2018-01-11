from mpp.models.sql_tc import SQLTestCase
from tinctest.models.scenario import ScenarioTestCase
from gppylib.commands.base import Command
import datetime, os, random, shutil, tinctest

def _set_VLIM_SLIM_REDZONEPERCENT(vlimMB, slimMB, activationPercent):

    # Set up GUCs for VLIM (gp_vmem_protect_limit), SLIM (gp_vmem_limit_per_query) and RQT activation percent (runaway_detector_activation_percent)
    tinctest.logger.info('Setting GUCs for VLIM gp_vmem_protect_limit=%dMB, SLIM gp_vmem_limit_per_query=%dMB and RQT activation percent runaway_detector_activation_percent=%s'%(vlimMB, slimMB, activationPercent))
    Command('Run gpconfig to set GUC gp_vmem_protect_limit',
            'source $GPHOME/greenplum_path.sh;gpconfig -c gp_vmem_protect_limit -v %d' % vlimMB).run(validateAfter=True)
    Command('Run gpconfig to set GUC gp_vmem_limit_per_query',
            'source $GPHOME/greenplum_path.sh;gpconfig -c gp_vmem_limit_per_query -v %d --skipvalidation' % (slimMB * 1024)).run(validateAfter=True)
    Command('Run gpconfig to set GUC runaway_detector_activation_percent',
            'source $GPHOME/greenplum_path.sh;gpconfig -c runaway_detector_activation_percent -v %d --skipvalidation' % activationPercent).run(validateAfter=True)

    # Restart DB
    Command('Restart database for GUCs to take effect', 
            'source $GPHOME/greenplum_path.sh && gpstop -air').run(validateAfter=True)

def _reset_VLIM_SLIM_REDZONEPERCENT():

    # Reset GUCs for VLIM (gp_vmem_protect_limit), SLIM (gp_vmem_limit_per_query) and RQT activation percent (runaway_detector_activation_percent)
    tinctest.logger.info('Resetting GUCs for VLIM gp_vmem_protect_limit, SLIM gp_vmem_limit_per_query, and RQT activation percent runaway_detector_activation_percent')
    Command('Run gpconfig to reset GUC gp_vmem_protect_limit',
            'source $GPHOME/greenplum_path.sh;gpconfig -c gp_vmem_protect_limit -v 8192').run(validateAfter=True)
    Command('Run gpconfig to reset GUC gp_vmem_limit_per_query',
            'source $GPHOME/greenplum_path.sh;gpconfig -r gp_vmem_limit_per_query --skipvalidation').run(validateAfter=True)
    Command('Run gpconfig to reset GUC runaway_detector_activation_percent',
            'source $GPHOME/greenplum_path.sh;gpconfig -r runaway_detector_activation_percent --skipvalidation').run(validateAfter=True)
    # Restart DB
    Command('Restart database for GUCs to take effect', 
            'source $GPHOME/greenplum_path.sh && gpstop -air').run(validateAfter=True)

class RQTBaseScenarioTestCase(ScenarioTestCase):
    
    def _infer_metadata(self):
        """
        Adding two new metadata 
        1. concurrency specify the number of concurrent sessions
        2. rqt_sql_test_case specify the class path of SQLTestCase that will be used for this scenario testcase
        """
        super(RQTBaseScenarioTestCase, self)._infer_metadata()
        try:
            self.concurrency = int(self._metadata.get('concurrency', '10'))
            self.rqt_sql_test_case = self._metadata.get('rqt_sql_test_case', 'resource_management.runaway_query.runaway_query_scenario.RQTBaseSQLTestCase')
        except:
            raise
    
    def test_run(self):
        # setup
        test_case_setup = []
        test_case_setup.append('%s.test_setup'%(self.rqt_sql_test_case))
        self.test_case_scenario.append(test_case_setup)
        
        # run testcase
        test_case_run = []
        for count in range(self.concurrency):
            test_case_run.append('%s.test_run'%(self.rqt_sql_test_case))
        self.test_case_scenario.append(test_case_run)
        
        # teardown
        test_case_teardown = []
        test_case_teardown.append('%s.test_teardown'%(self.rqt_sql_test_case))
        self.test_case_scenario.append(test_case_teardown)

class RQTBaseSQLTestCase(SQLTestCase):
    """
    RQTBaseSQLTestCase is used in RQTBaseScenarioTestCase for specifying how we iteratively
    run sql files for one session
    """

    sql_dir = 'sql/'
    out_dir = 'output/'
    
    def _infer_metadata(self):
        """
        We introduce four new metadata 
        1. vlimMB specify vmem protect limit
        2. slimMB specify vmem limit per query
        3. redzone specify red zone detection percentage range from 0 ~ 100
        4. iterations specify how many iterations we will run 
        """
        super(RQTBaseSQLTestCase, self)._infer_metadata()
        try:
            self.vlimMB = int(self._metadata.get('vlimMB', '8192')) # Default is 8192
            self.slimMB = int(self._metadata.get('slimMB', '0')) # Default is 0
            self.redzone_percent = int(self._metadata.get('redzone', '80')) # Default is 80
            self.iterations = int(self._metadata.get('iterations', '10')) # Default is 10
        except:
            raise
    
    def test_setup(self):
        """
        global setup function for the testcase 
        The default one will setup the vlimMB, slimMB and runaway_detector_activation_percent
        Overwirte it if you need to add more setup such as creating new tables and loading new data
        """
        _set_VLIM_SLIM_REDZONEPERCENT(self.vlimMB, self.slimMB, self.redzone_percent)
        self.__cleanup_out_dir()
        
    def get_next_sql(self, sql_files, iteration_count):
        """
        Pick the next sql file to run in this session
        By default we use random algorithm
        Rewrite this function to support others
        @arg sql_files list of sql files detected in the sql_dir
        @arg iteration_count count of the current iteration 
        """
        return sql_files[random.randint(0, len(sql_files) - 1)]
    
    def test_run(self):
        """
        For each iteration, pick a sql file and run. 
        """
        sql_files = os.listdir(self.get_sql_dir())
        for iter in range(1, self.iterations+1):
            sql_file = self.get_next_sql(sql_files, iter)
            current_timestamp = datetime.datetime.now().strftime('%Y-%m-%d-%H-%M-%S-%f')
            run_sql_file = os.path.join(self.get_out_dir(), '%s-iter%s-%s'%(current_timestamp, iter, sql_file))
            shutil.copyfile(os.path.join(self.get_sql_dir(),sql_file), run_sql_file)
            run_out_file = run_sql_file.replace('.sql','.out')
            run_sql_cmd = 'psql -d %s -a -f %s &> %s'%(self.db_name, run_sql_file, run_out_file)
            Command('Run Sql File', run_sql_cmd).run(validateAfter=True)     
            end_timestamp = datetime.datetime.now().strftime('%Y-%m-%d-%H-%M-%S-%f')
            with open(run_out_file, 'a') as o:
                o.write('\nFINISH_TIME:%s'%end_timestamp)
                
    def __cleanup_out_dir(self):
        """
        Remove existing ans and sql files in out dir to avoid wrong results
        """
        for ff in os.listdir(self.get_out_dir()):
            if ff.endswith('.sql') or ff.endswith('.out') or ff.endswith('.txt'):
                os.remove(os.path.join(self.get_out_dir(), ff))
                
    def __summary_result(self):
        """
        Summarize the concurrency test into runtime.txt file 
        """
        
        test_fail = False
        timeline = {}
        for ff in os.listdir(self.get_out_dir()):
            if ff.endswith('.out'):
                query_name = ff[ff.index('iter'):ff.index('.')]
                temp_line = ff[:ff.index('-iter')]
                start_time = temp_line[:temp_line.rindex('-')]
                self.__update_result(timeline, start_time, query_name, 0)
        
                lines = [line.strip() for line in open(os.path.join(self.get_out_dir(), ff))]
                b_error = False
                error_reason = ''
                finish_time = ''
                for line in lines:
                    if line.find('ERROR') != -1:
                        b_error = True
                        if line.find('Out of memory') != -1:
                            error_reason = 'OOM'
                            test_fail = True
                        elif line.find('red zone') != -1:
                            error_reason = 'Hit red zone'
                        elif line.find('current transaction is aborted') != -1:
                            continue
                        else:
                            error_reason = 'Unexpected'
                            test_fail = True
                    elif line.find('FINISH_TIME') != -1:
                        finish_time = line[line.index(':')+1:line.rindex('-')]
                if b_error:
                    self.__update_result(timeline, finish_time, '%s (%s)'%(query_name, error_reason), 2)
                else:
                    self.__update_result(timeline, finish_time, query_name, 1)
        result = ''
        
        arr_timeline = []
        arr_timeline.extend(timeline.keys())
        arr_timeline.sort()
        
        result += self.__print_timeline(arr_timeline[0], timeline[arr_timeline[0]])
        for i in range(1, len(arr_timeline)):
            cur_time = arr_timeline[i]
            result +='|\n|\n'
            result += self.__print_timeline(cur_time, timeline[cur_time])
            
        with open(os.path.join(self.get_out_dir(),'runtime.txt'), 'w') as o:
            o.write(result)
        if test_fail:
            raise Exception("Test failed: error reason %s" % error_reason)
        
        
    def __print_timeline(self, timestamp, bullet):
        result = 'Time:%s\n'%timestamp
        if len(bullet['query_start']) != 0:
            result+= 'Query started (%s):%s\n'%(len(bullet['query_start']), ','.join(bullet['query_start']))
        if len(bullet['query_finish']) != 0:
            result+= 'Query finished (%s):%s\n'%(len(bullet['query_finish']), ','.join(bullet['query_finish']))
        if len(bullet['query_error']) != 0:
            result+= 'Query errored (%s):%s\n'%(len(bullet['query_error']), ','.join(bullet['query_error']))
        return result

    def test_teardown(self):
        self.__summary_result()
        _reset_VLIM_SLIM_REDZONEPERCENT()
        
    def __update_result(self, dict_timeline, timestamp, query_name, update_type):
        if timestamp not in dict_timeline.keys():
            dict_timeline[timestamp] = {}
            dict_timeline[timestamp]['query_start'] = []
            dict_timeline[timestamp]['query_finish'] = []
            dict_timeline[timestamp]['query_error'] = []
            
        if update_type == 0:
            dict_timeline[timestamp]['query_start'].append(query_name)
        elif update_type == 1:
            dict_timeline[timestamp]['query_finish'].append(query_name)
        elif update_type == 2:
            dict_timeline[timestamp]['query_error'].append(query_name)
        else:
            raise Exception("Invalid update type: %d" % update_type)
