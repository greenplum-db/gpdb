import inspect
import os

from mpp.models import SQLTestCase

class SQLTemplateTestCase(SQLTestCase):
    def __init__(self, methodName, baseline_result = None, sql_file=None, db_name = None):
        PATH = os.path.dirname(inspect.getfile(self.__class__))
        PGUSER = os.environ.get("PGUSER")
        if PGUSER is None:
            PGUSER = os.environ.get("LOGNAME")
        self.transforms= {"%PGUSER%":PGUSER, "%MYD%":PATH, "%schema%":"public"}
        super(SQLTemplateTestCase, self).__init__(methodName, baseline_result, sql_file, db_name)

    @staticmethod
    def perform_transformation_on_sqlfile(input_filename, output_filename,transforms):
        #sql_template_test_case = SQLTemplateTestCase(methodName)
        with open(input_filename, 'r') as input:
            with open(output_filename, 'w') as output:
                for line in input.readlines():
                    for key,value in transforms.iteritems():
                        if key in line:
                            line = line.replace(key, value)
                    output.write(line)
        return output_filename

    def run_test(self):
        sql_file = self.sql_file
        ans_file = self.ans_file
        new_sql_file = sql_file + '.t'
        new_ans_file = ans_file + '.t'
        self.sql_file = transformed_sql_file = SQLTemplateTestCase.perform_transformation_on_sqlfile(sql_file, new_sql_file,self.transforms)
        self.ans_file = transformed_ans_file = SQLTemplateTestCase.perform_transformation_on_sqlfile(ans_file, new_ans_file,self.transforms)
        return super(SQLTemplateTestCase, self).run_test()
